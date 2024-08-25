/*
 * Button driver for Pico Pi
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024 Joshua Watt
 */

#ifndef _BUTTON_H_
#define _BUTTON_H_

#include <stdbool.h>
#include <stdint.h>

struct button;

struct button* button_create(unsigned int pin, bool invert,
                             unsigned int debounce_ms);
void button_free(struct button* b);
void button_set_repeat(struct button* b, unsigned int repeat_delay_ms,
                       unsigned int repeat_ms);
void button_update(struct button* b);
bool button_down(struct button const* b);
bool button_up(struct button const* b);
bool button_is_pressed(struct button const* b);
uint32_t button_last_duration_us(struct button const* b);
uint32_t button_current_duration_us(struct button const* b);
unsigned int button_repeat(struct button* b);

#endif
