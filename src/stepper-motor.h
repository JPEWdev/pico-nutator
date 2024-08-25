/*
 * Stepper motor driver for Pico Pi
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024 Joshua Watt
 */
#ifndef _STEPPER_MOTOR_H_
#define _STEPPER_MOTOR_H_

#include <stdbool.h>
#include <stdint.h>

struct stepper;

enum stepper_mode {
    STEPPER_MODE_WAVE = 0, /* E.g. single phase */
    STEPPER_MODE_DUAL_PHASE = 1,
    STEPPER_MODE_HALF_STEP = 2,
};

struct stepper* stepper_create(unsigned int steps_per_rev, unsigned int max_rpm,
                               enum stepper_mode mode, int enable_pin);

void stepper_add_pin(struct stepper* s, unsigned int pin, bool is_pwm);
void stepper_set_accel(struct stepper* s, unsigned int rpm_per_sec,
                       unsigned int min_rpm);
void stepper_step(struct stepper* s, bool forward);
bool stepper_update(struct stepper* s);
void stepper_brake(struct stepper* s);
void stepper_hold(struct stepper* s);
void stepper_enable(struct stepper* s, bool enable);
void stepper_set_rpm(struct stepper* s, unsigned int rpm);
unsigned int stepper_get_rpm(struct stepper const* s);
unsigned int stepper_get_actual_rpm(struct stepper const* s);
uint64_t stepper_step_count(struct stepper const* s);

#endif
