/*
 * Pico Pi driver for the Newhaven Display K3Z family of LCDs
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024 Joshua Watt
 */
#ifndef _NHD_K3Z_H_
#define _NHD_K3Z_H_

#include <stdarg.h>
#include <stdbool.h>

#include "pico/stdlib.h"

enum nhdk3z_baud {
    NHDK3Z_BAUD_300 = 1,
    NHDK3Z_BAUD_1200,
    NHDK3Z_BAUD_2400,
    NHDK3Z_BAUD_9600,
    NHDK3Z_BAUD_14400,
    NHDK3Z_BAUD_19200,
    NHDK3Z_BAUD_57600,
    NHDK3Z_BAUD_115200,
};

struct nhdk3z;

struct nhdk3z* nhdk3z_create(uart_inst_t* uart);
void nhdk3z_free(struct nhdk3z* d);
void nhdk3z_set_baud(struct nhdk3z* d, enum nhdk3z_baud baud);
void nhdk3z_write(struct nhdk3z* d, char const* s);
void nhdk3z_vprintf(struct nhdk3z* d, char const* format, va_list args);
void nhdk3z_printf(struct nhdk3z* d, char const* format, ...);
void nhdk3z_clear(struct nhdk3z* d);
void nhdk3z_home(struct nhdk3z* d);
void nhdk3z_set_cursor(struct nhdk3z* d, uint8_t pos);
void nhdk3z_set_contrast(struct nhdk3z* d, uint8_t contrast);
void nhdk3z_set_brightness(struct nhdk3z* d, uint8_t brightness);
void nhdk3z_set_cursor_blink(struct nhdk3z* d, bool blink);
void nhdk3z_set_cursor_underline(struct nhdk3z* d, bool underline);
void nhdk3z_set_display_on(struct nhdk3z* d, bool on);

#endif
