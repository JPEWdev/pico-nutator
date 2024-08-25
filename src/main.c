/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024 Joshua Watt
 */
#include <inttypes.h>
#include <stdio.h>

#include "button.h"
#include "hardware/pwm.h"
#include "nhd-k3z.h"
#include "persist.h"
#include "pico/stdlib.h"
#include "stepper-motor.h"

#define VERSION "1.0"

#define MAX_RPM (60)
#define RPM_STEP (5)
#define STEPS_PER_REV (200)
#define SLEEP_TIMEOUT_US (60 * 1000000)

/*
 * Frequency is high so that the stepper motor is (more or less) not audible
 * when holding
 */
#define MOTOR_FREQUENCY (15000)

/*
 * Power supply is 12V, the motor is rated for 1.5 Amps max, with a resistance
 * of 2.3 Ohms. In an ideal world, this would normally be a 28% duty cycle,
 * however the frequency of our PWM is quite a bit above the cutoff frequency
 * of the motor (4 mH inductance, cutoff is 91 Hz), so lot of attenuation is
 * happening, and there is a pretty non-linear response to the duty cycle
 * because of this.
 *
 * As such, this was determined empirically, mostly by checking if the stepper
 * motor driver was too hot to touch
 */
#define MOTOR_DUTY_CYCLE (40)

#define MOTOR_ACCEL (60)

#define LED_PIN (25)

#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))

/*
 * Motor uses even pins so that each can has its own independent PWM. This is
 * in case I want to do micro-stepping some day
 */
static const unsigned int motor_pins[] = {0, 4, 2, 6};

/*
 * Motor enable pin is also even in case we want to independently PWM it
 */
#define MOTOR_ENABLE_PIN (8)

/*
 * Fan pin is also even in case we want to independently PWM it
 */
#define FAN_PIN (10)

#define DISPLAY_PIN (12)
#define DISPLAY_UART (uart0)

#define START_STOP_BTN_PIN (13)
#define DOWN_BTN_PIN (14)
#define UP_BTN_PIN (15)

static struct button* make_button(int pin) {
    struct button* b = button_create(pin, true, 35);
    gpio_pull_up(pin);
    button_set_repeat(b, 1000, 500);
    return b;
}

bool run = false;
uint64_t run_time_start = 0;
bool sleeping = false;
struct nhdk3z* display;
struct stepper* motor;

struct persist persist;

struct hms {
    unsigned int hours;
    unsigned int minutes;
    unsigned int seconds;
};

static struct hms us_to_hms(uint64_t us) {
    struct hms result;

    result.hours = us / (1000000ull * 60 * 60);
    result.minutes = (us / (1000000ull * 60)) % 60;
    result.seconds = (us / 1000000ull) % 60;

    return result;
}

static void set_target_rpm(unsigned int new_rpm) {
    new_rpm = MAX(new_rpm, RPM_STEP);
    new_rpm = MIN(new_rpm, MAX_RPM);

    persist.target_rpm = new_rpm;
    if (run) {
        stepper_set_rpm(motor, persist.target_rpm);
    }

    printf("Target RPM is now %" PRIu32 "\n", persist.target_rpm);
}

static void update_display() {
    if (sleeping) {
        return;
    }

    nhdk3z_clear(display);
    nhdk3z_home(display);
    if (run) {
        struct hms hms = us_to_hms(time_us_64() - run_time_start);

        nhdk3z_printf(display, "Running %u:%02u:%02u", hms.hours, hms.minutes,
                      hms.seconds);
    } else {
        nhdk3z_write(display, "Stopped");
    }
    nhdk3z_set_cursor(display, 0x40);
    nhdk3z_printf(display, "RPM %d", persist.target_rpm);
    if (run) {
        unsigned int actual_rpm = stepper_get_actual_rpm(motor);
        if (actual_rpm && actual_rpm != persist.target_rpm) {
            nhdk3z_printf(display, " (%d%%)",
                          100 * actual_rpm / persist.target_rpm);
        }
    }
}

static void set_sleep(bool sleep) {
    if (sleeping == sleep) {
        return;
    }

    sleeping = sleep;

    stepper_enable(motor, !sleeping);
    if (sleeping) {
        nhdk3z_set_brightness(display, 1);
        gpio_put(FAN_PIN, 0);
    } else {
        nhdk3z_set_brightness(display, 8);
        stepper_hold(motor);
        gpio_put(FAN_PIN, 1);
        update_display();
    }
}

static uint32_t pwm_set_freq_duty(unsigned int slice_num, unsigned int chan,
                                  uint32_t frequency, int duty) {
    uint32_t clock = 125000000;
    uint32_t divider16 =
        clock / frequency / 4096 + (clock % (frequency * 4096) != 0);
    if (divider16 / 16 == 0) {
        divider16 = 16;
    }
    uint32_t wrap = clock * 16 / divider16 / frequency - 1;
    pwm_set_clkdiv_int_frac(slice_num, divider16 / 16, divider16 & 0xF);
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, chan, wrap * duty / 100);
    return wrap;
}

int main() {
    stdio_init_all();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);
    printf("Booting...");
    /* Wait for display to power up */
    sleep_ms(1000);
    read_persist(&persist);

    /* Buttons */
    struct button* up_button = make_button(UP_BTN_PIN);
    struct button* down_button = make_button(DOWN_BTN_PIN);
    struct button* start_stop_button = make_button(START_STOP_BTN_PIN);

    /* Fan */
    gpio_init(FAN_PIN);
    gpio_set_dir(FAN_PIN, GPIO_OUT);
    gpio_put(FAN_PIN, 0);

    /* Motor */
    /*
     * The motor is driven in half-step mode. This results in uneven torque and
     * lower average torque than dual phase stepping, since each alternating
     * step uses either 1 or 2 phases of the motor. However, the motor runs
     * much smoother since it effectively doubles the number of steps
     */
    motor = stepper_create(STEPS_PER_REV, MAX_RPM, STEPPER_MODE_HALF_STEP,
                           MOTOR_ENABLE_PIN);

    uint32_t pwm_mask = 0;
    for (int i = 0; i < ARRAY_COUNT(motor_pins); i++) {
        unsigned int slice_num = pwm_gpio_to_slice_num(motor_pins[i]);
        unsigned int chan = pwm_gpio_to_channel(motor_pins[i]);
        pwm_set_freq_duty(slice_num, chan, MOTOR_FREQUENCY, MOTOR_DUTY_CYCLE);
        stepper_add_pin(motor, motor_pins[i], true);
        pwm_mask |= 1 << slice_num;
    }
    pwm_set_mask_enabled(pwm_mask);

    /* Display */
    display = nhdk3z_create(DISPLAY_UART);
    gpio_set_function(DISPLAY_PIN, GPIO_FUNC_UART);
    nhdk3z_set_baud(display, NHDK3Z_BAUD_57600);

    nhdk3z_set_display_on(display, true);
    nhdk3z_set_contrast(display, 50);
    nhdk3z_set_brightness(display, 8);
    nhdk3z_set_cursor_blink(display, false);
    nhdk3z_set_cursor_underline(display, false);
    nhdk3z_clear(display);
    nhdk3z_home(display);
    nhdk3z_printf(display, "Version %s", VERSION);
    sleep_ms(2000);

    stepper_set_accel(motor, MOTOR_ACCEL, RPM_STEP);
    stepper_enable(motor, true);
    stepper_hold(motor);
    update_display();
    gpio_put(FAN_PIN, 1);

    uint64_t sleep_start = time_us_64();
    int run_time_sec = 0;

    while (true) {
        uint64_t now = time_us_64();
        bool redraw = false;

        if (!run && !sleeping && now >= sleep_start + SLEEP_TIMEOUT_US) {
            set_sleep(true);
        }

        /*
         * Redraw if running and the seconds have changed
         */
        if (run) {
            struct hms hms = us_to_hms(now - run_time_start);
            if (hms.seconds != run_time_sec) {
                redraw = true;
                run_time_sec = hms.seconds;
            }
        }

        gpio_put(LED_PIN, stepper_update(motor) ? 1 : 0);
        button_update(up_button);
        button_update(down_button);
        button_update(start_stop_button);

        if (sleeping) {
            if (button_up(up_button) || button_up(down_button) ||
                button_up(start_stop_button)) {
                set_sleep(false);
                sleep_start = now;
            }
        } else {
            if (button_repeat(up_button)) {
                set_target_rpm(persist.target_rpm + RPM_STEP);
                sleep_start = now;
                redraw = true;
            }

            if (button_repeat(down_button)) {
                set_target_rpm(persist.target_rpm - RPM_STEP);
                sleep_start = now;
                redraw = true;
            }

            if (!run && button_is_pressed(start_stop_button) &&
                button_current_duration_us(start_stop_button) >= 4000000) {
                nhdk3z_clear(display);
                nhdk3z_home(display);
                nhdk3z_write(display, "Sleeping...");
                sleep_ms(1000);
                set_sleep(true);
                while (!button_up(start_stop_button)) {
                    button_update(start_stop_button);
                }
            } else if (button_up(start_stop_button)) {
                run = !run;
                write_persist(&persist);
                if (run) {
                    stepper_set_rpm(motor, persist.target_rpm);
                    run_time_start = now;
                    run_time_sec = 0;
                } else {
                    stepper_set_rpm(motor, 0);
                }
                sleep_start = now;
                redraw = true;
            }
        }

        if (redraw) {
            update_display();
        }
    }

    return 0;
}
