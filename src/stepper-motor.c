/*
 * Stepper motor driver for Pico Pi
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024 Joshua Watt
 */
#include "stepper-motor.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "pico/stdlib.h"

#define US_PER_SEC (1000000ull)
#define US_PER_MIN (60 * US_PER_SEC)

#define MIN_RPM (1ull)

struct stepper {
    unsigned int steps_per_rev;
    unsigned int max_rpm;
    enum stepper_mode mode;
    unsigned int mask;
    unsigned int half_mask;
    unsigned int target_rpm;
    unsigned int accel_rpm_per_sec;
    int enable_pin;
    size_t num_pins;
    struct {
        unsigned int pin;
        bool is_pwm;
    }* pins;
    uint64_t last_step;
    uint64_t us_per_step_target;
    uint64_t us_per_step;
    uint64_t us_accel;
    uint64_t max_us_per_step;
    uint64_t last_accel_step;
    uint64_t step_count;
};

static uint64_t rpm_to_step_us(struct stepper const* s, unsigned int rpm) {
    return US_PER_MIN / ((uint64_t)rpm * s->steps_per_rev);
}

static void update(struct stepper const* s) {
    uint32_t mask = 0;
    uint32_t value = 0;
    for (size_t i = 0; i < s->num_pins; i++) {
        mask |= 1 << s->pins[i].pin;

        if (((s->mask | s->half_mask) >> i) & 0x1) {
            if (s->pins[i].is_pwm) {
                gpio_set_function(s->pins[i].pin, GPIO_FUNC_PWM);
            } else {
                value |= 1 << s->pins[i].pin;
            }
        } else if (s->pins[i].is_pwm) {
            gpio_set_function(s->pins[i].pin, GPIO_FUNC_SIO);
        }
    }
    gpio_put_masked(mask, value);
}

static uint32_t step_mask(uint32_t mask, bool forward, size_t num_pins) {
    if (forward) {
        if (mask & 0x1) {
            mask |= (1 << num_pins);
        }
        mask >>= 1;
    } else {
        mask <<= 1;
        if (mask & (1 << num_pins)) {
            mask |= 1;
        }
    }
    mask &= (1 << num_pins) - 1;

    return mask;
}

static void step(struct stepper* s, bool forward) {
    if (!s->mask) {
        stepper_hold(s);
        return;
    }

    /*
     * For half step, move the main mask on odd steps, and the half mask on
     * even steps
     */
    if (s->mode != STEPPER_MODE_HALF_STEP || (s->step_count & 1)) {
        s->mask = step_mask(s->mask, forward, s->num_pins);
    } else {
        s->half_mask = step_mask(s->half_mask, forward, s->num_pins);
    }

    s->step_count++;
    update(s);
}

struct stepper* stepper_create(unsigned int steps_per_rev, unsigned int max_rpm,
                               enum stepper_mode mode, int enable_pin) {
    struct stepper* s = calloc(1, sizeof(*s));
    s->steps_per_rev = steps_per_rev;
    if (mode == STEPPER_MODE_HALF_STEP) {
        s->steps_per_rev *= 2;
    }
    s->max_rpm = max_rpm;
    s->mode = mode;
    s->enable_pin = enable_pin;
    if (enable_pin >= 0) {
        gpio_init(enable_pin);
        gpio_set_dir(enable_pin, GPIO_OUT);
        gpio_put(enable_pin, 0);
    }
    return s;
}

void stepper_free(struct stepper* s) {
    for (size_t i = 0; i < s->num_pins; i++) {
        gpio_deinit(s->pins[i].pin);
    }
    if (s->enable_pin >= 0) {
        gpio_deinit(s->enable_pin);
    }
    free(s->pins);
    free(s);
}

void stepper_add_pin(struct stepper* s, unsigned int pin, bool is_pwm) {
    s->pins = realloc(s->pins, sizeof(*s->pins) * (s->num_pins + 1));
    s->pins[s->num_pins].pin = pin;
    s->pins[s->num_pins].is_pwm = is_pwm;
    s->num_pins++;

    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 0);
}

void stepper_set_accel(struct stepper* s, unsigned int rpm_per_sec,
                       unsigned int min_rpm) {
    if (rpm_per_sec == 0) {
        s->us_accel = 0;
    } else {
        s->us_accel = rpm_to_step_us(s, rpm_per_sec * 60);
        s->max_us_per_step = rpm_to_step_us(s, min_rpm);
    }
}

void stepper_step(struct stepper* s, bool forward) {
    step(s, forward);
    s->last_step = time_us_64();
    s->last_accel_step = s->last_step;
}

bool stepper_update(struct stepper* s) {
    uint64_t now = time_us_64();

    if (s->us_accel) {
        if (s->us_per_step_target == 0 &&
            (s->us_per_step == s->max_us_per_step || s->us_per_step == 0)) {
            s->us_per_step = 0;
        } else if (s->us_per_step_target != 0 && s->us_per_step == 0) {
            s->us_per_step = s->max_us_per_step;
        } else {
            if (now >= s->last_accel_step) {
                int num_steps = (now - s->last_accel_step) / s->us_accel;
                int64_t target = s->us_per_step_target ? s->us_per_step_target
                                                       : s->max_us_per_step;

                if (s->us_per_step < target) {
                    s->us_per_step =
                        MIN((int64_t)s->us_per_step + num_steps, target);
                } else if (s->us_per_step > target) {
                    s->us_per_step =
                        MAX((int64_t)s->us_per_step - num_steps, target);
                }

                s->last_accel_step += s->us_accel * num_steps;
            }
        }
    } else {
        s->us_per_step = s->us_per_step_target;
    }

    if (!s->us_per_step) {
        return false;
    }

    if (now >= s->last_step) {
        int num_steps = (now - s->last_step) / s->us_per_step;
        if (num_steps) {
            step(s, true);
            s->last_step += s->us_per_step;
        }

        return num_steps > 1;
    }
    return false;
}

void stepper_brake(struct stepper* s) {
    s->mask = 0;
    s->half_mask = 0;
    update(s);
}

void stepper_hold(struct stepper* s) {
    switch (s->mode) {
        case STEPPER_MODE_WAVE:
            s->mask = 0x1;
            s->half_mask = 0x0;
            break;

        case STEPPER_MODE_DUAL_PHASE:
            s->mask = 0x3;
            s->half_mask = 0x0;
            break;

        case STEPPER_MODE_HALF_STEP:
            /*
             * Both masks must start on the same pin since it is random which
             * one will advance first
             */
            s->mask = 0x1;
            s->half_mask = 0x1;
            break;
    }
    update(s);
}

void stepper_enable(struct stepper* s, bool enable) {
    if (s->enable_pin >= 0) {
        gpio_put(s->enable_pin, enable ? 1 : 0);
    }
}

void stepper_set_rpm(struct stepper* s, unsigned int rpm) {
    rpm = MIN(rpm, s->max_rpm);

    if (rpm == s->target_rpm) {
        return;
    }

    s->target_rpm = rpm;
    s->last_step = time_us_64();
    s->last_accel_step = time_us_64();
    if (rpm) {
        s->us_per_step_target = rpm_to_step_us(s, rpm);
    } else {
        s->us_per_step_target = 0;
    }
}

unsigned int stepper_get_rpm(struct stepper const* s) { return s->target_rpm; }

unsigned int stepper_get_actual_rpm(struct stepper const* s) {
    if (!s->us_per_step) {
        return 0;
    }
    return US_PER_MIN / (s->us_per_step * s->steps_per_rev);
}

uint64_t stepper_step_count(struct stepper const* s) { return s->step_count; }
