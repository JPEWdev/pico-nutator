/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024 Joshua Watt
 */
#ifndef _PERSIST_H
#define _PERSIST_H

#include <stdint.h>

#define PERSIST_VERSION 1

struct persist {
    uint32_t version;
    uint32_t target_rpm;
};

void read_persist(struct persist* p);
void write_persist(struct persist const* p);

#endif

