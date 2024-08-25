/*
 * Pico Pi driver for the Newhaven Display K3Z family of LCDs
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024 Joshua Watt
 */
#include "nhd-k3z.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"

struct nhdk3z {
    uart_inst_t* uart;
};

struct nhdk3z* nhdk3z_create(uart_inst_t* uart) {
    struct nhdk3z* d = calloc(1, sizeof(*d));

    d->uart = uart;
    uart_init(uart, 9600);
    uart_set_hw_flow(uart, false, false);
    uart_set_format(uart, 8, 1, UART_PARITY_NONE);

    return d;
}

void nhdk3z_free(struct nhdk3z* d) {
    uart_deinit(d->uart);
    free(d);
}

void nhdk3z_set_baud(struct nhdk3z* d, enum nhdk3z_baud baud) {
    const uint8_t cmd[] = {0xfe, 0x61, baud};
    uart_write_blocking(d->uart, cmd, sizeof(cmd));
    uart_tx_wait_blocking(d->uart);
    switch (baud) {
        case NHDK3Z_BAUD_300:
            uart_set_baudrate(d->uart, 300);
            break;
        case NHDK3Z_BAUD_1200:
            uart_set_baudrate(d->uart, 1200);
            break;
        case NHDK3Z_BAUD_2400:
            uart_set_baudrate(d->uart, 2400);
            break;
        case NHDK3Z_BAUD_9600:
            uart_set_baudrate(d->uart, 9600);
            break;
        case NHDK3Z_BAUD_14400:
            uart_set_baudrate(d->uart, 14400);
            break;
        case NHDK3Z_BAUD_19200:
            uart_set_baudrate(d->uart, 19200);
            break;
        case NHDK3Z_BAUD_57600:
            uart_set_baudrate(d->uart, 57600);
            break;
        case NHDK3Z_BAUD_115200:
            uart_set_baudrate(d->uart, 115200);
            break;
    }
    sleep_us(20);
}

void nhdk3z_write(struct nhdk3z* d, char const* s) {
    uart_write_blocking(d->uart, (uint8_t const*)s, strlen(s));
}

void nhdk3z_vprintf(struct nhdk3z* d, char const* format, va_list args) {
    va_list arg_copy;
    va_copy(arg_copy, args);

    char buffer[vsnprintf(NULL, 0, format, args) + 1];

    vsnprintf(buffer, sizeof(buffer), format, arg_copy);

    nhdk3z_write(d, buffer);
    va_end(arg_copy);
}

void nhdk3z_printf(struct nhdk3z* d, char const* format, ...) {
    va_list args;
    va_start(args, format);
    nhdk3z_vprintf(d, format, args);
    va_end(args);
}

void nhdk3z_clear(struct nhdk3z* d) {
    static const uint8_t cmd[] = {0xfe, 0x51};
    uart_write_blocking(d->uart, cmd, sizeof(cmd));
}

void nhdk3z_home(struct nhdk3z* d) {
    static const uint8_t cmd[] = {0xfe, 0x46};
    uart_write_blocking(d->uart, cmd, sizeof(cmd));
}

void nhdk3z_set_cursor(struct nhdk3z* d, uint8_t pos) {
    const uint8_t cmd[] = {0xfe, 0x45, pos};
    uart_write_blocking(d->uart, cmd, sizeof(cmd));
}

void nhdk3z_set_contrast(struct nhdk3z* d, uint8_t contrast) {
    contrast = MIN(contrast, 50);
    contrast = MAX(contrast, 1);
    const uint8_t cmd[] = {0xfe, 0x52, contrast};
    uart_write_blocking(d->uart, cmd, sizeof(cmd));
}

void nhdk3z_set_brightness(struct nhdk3z* d, uint8_t brightness) {
    brightness = MIN(brightness, 8);
    brightness = MAX(brightness, 1);

    const uint8_t cmd[] = {0xfe, 0x53, brightness};
    uart_write_blocking(d->uart, cmd, sizeof(cmd));
}

void nhdk3z_set_cursor_blink(struct nhdk3z* d, bool blink) {
    const uint8_t cmd[] = {0xfe, blink ? 0x4b : 0x4c};
    uart_write_blocking(d->uart, cmd, sizeof(cmd));
}

void nhdk3z_set_cursor_underline(struct nhdk3z* d, bool underline) {
    const uint8_t cmd[] = {0xfe, underline ? 0x47 : 0x48};
    uart_write_blocking(d->uart, cmd, sizeof(cmd));
}

void nhdk3z_set_display_on(struct nhdk3z* d, bool on) {
    const uint8_t cmd[] = {0xfe, on ? 0x41 : 0x42};
    uart_write_blocking(d->uart, cmd, sizeof(cmd));
}

