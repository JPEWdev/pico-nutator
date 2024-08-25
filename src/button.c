/*
 * Button driver for Pico Pi
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024 Joshua Watt
 */
#include "button.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "pico/stdlib.h"

enum state {
    STATE_RELEASED,
    STATE_DEBOUNCE,
    STATE_PRESSED,
    STATE_REPEAT,
};

struct button {
    unsigned int pin;
    bool invert;
    unsigned int debounce_ms;
    uint32_t start_time;
    uint32_t last_duration;
    bool down;
    bool up;
    bool is_pressed;
    enum state state;

    unsigned int repeat_delay_ms;
    unsigned int repeat_ms;
    uint32_t last_repeat;
    unsigned int repeat_count;
};

struct button* button_create(unsigned int pin, bool invert,
                             unsigned int debounce_ms) {
    struct button* b = calloc(1, sizeof(*b));

    b->pin = pin;
    b->invert = invert;
    b->debounce_ms = debounce_ms;

    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);

    return b;
}

void button_free(struct button* b) {
    gpio_deinit(b->pin);
    free(b);
}

void button_set_repeat(struct button* b, unsigned int repeat_delay_ms,
                       unsigned int repeat_ms) {
    b->repeat_delay_ms = repeat_delay_ms;
    b->repeat_ms = repeat_ms;
}

void button_update(struct button* b) {
    bool pressed = b->invert ? !gpio_get(b->pin) : gpio_get(b->pin);

    uint32_t now = time_us_32();

    b->down = false;
    b->up = false;
    b->is_pressed = false;
    switch (b->state) {
        case STATE_RELEASED:
            if (pressed) {
                b->state = STATE_DEBOUNCE;
                b->start_time = now;
            }
            break;

        case STATE_DEBOUNCE:
            if (pressed) {
                if (now >= b->start_time + b->debounce_ms * 1000) {
                    b->down = true;
                    b->state = STATE_PRESSED;
                    b->start_time = now;
                    b->repeat_count = 1;
                }
            } else {
                b->state = STATE_RELEASED;
            }
            break;

        case STATE_REPEAT:
            if (b->repeat_ms) {
                while (b->last_repeat + b->repeat_ms * 1000 < now) {
                    b->repeat_count++;
                    b->last_repeat += b->repeat_ms * 1000;
                }
            }
            // Fallthrough

        case STATE_PRESSED:
            b->is_pressed = pressed;
            if (pressed) {
                if (b->state == STATE_PRESSED && b->repeat_delay_ms &&
                    now >= b->start_time + b->repeat_delay_ms * 1000) {
                    b->state = STATE_REPEAT;
                    b->last_repeat = now;
                    b->repeat_count++;
                }
            } else {
                b->up = true;
                b->state = STATE_RELEASED;
                b->last_duration = now - b->start_time;
            }
            break;
    }
}

bool button_down(struct button const* b) { return b->down; }

bool button_up(struct button const* b) { return b->up; }

bool button_is_pressed(struct button const* b) { return b->is_pressed; }

uint32_t button_last_duration_us(struct button const* b) {
    return b->last_duration;
}

uint32_t button_current_duration_us(struct button const* b) {
    return time_us_32() - b->start_time;
}

unsigned int button_repeat(struct button* b) {
    unsigned int ret = b->repeat_count;
    b->repeat_count = 0;
    return ret;
}
