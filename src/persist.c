/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024 Joshua Watt
 */
#include "persist.h"

#include <stdio.h>
#include <string.h>

#include "hardware/flash.h"
#include "hardware/sync.h"

#define ROUND_UP(_size, _factor) \
    (((_size) + (_factor) - 1) - ((_size) + (_factor) - 1) % (_factor))

#define DEFAULT_PERSIST  \
    {                    \
        PERSIST_VERSION, \
        20,              \
    }

#define PERSIST_OFFSET ((uintptr_t)(&persist) - XIP_BASE)

static struct persist __attribute__((section(".section_persist"))) persist =
    DEFAULT_PERSIST;

void read_persist(struct persist* p) {
    if (persist.version != PERSIST_VERSION) {
        static const struct persist default_persist = DEFAULT_PERSIST;
        *p = default_persist;
        return;
    }
    *p = persist;
}

void write_persist(struct persist const* p) {
    uint8_t buffer[ROUND_UP(sizeof(*p), FLASH_PAGE_SIZE)];

    memset(buffer, 0xFF, sizeof(buffer));
    memcpy(buffer, p, sizeof(*p));

    if (memcmp(buffer, &persist, sizeof(persist)) != 0) {
        uint32_t interrupts = save_and_disable_interrupts();
        flash_range_erase(PERSIST_OFFSET,
                          ROUND_UP(sizeof(buffer), FLASH_SECTOR_SIZE));
        flash_range_program(PERSIST_OFFSET, buffer, sizeof(buffer));
        restore_interrupts(interrupts);
    }
}

