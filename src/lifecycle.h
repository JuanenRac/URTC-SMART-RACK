// =============================================================================
// URTC-SMART-RACK Firmware - Tool lifecycle tracking: lifecycle.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
// The README's "Lifecycle Logs" Key Feature: accumulating actuation
// cycles and usage time per tool. The accumulation math itself doesn't
// need the F-RAM this will eventually persist into - a lifecycle_t is
// just two counters, real and testable in a plain host build today.
#ifndef LIFECYCLE_H
#define LIFECYCLE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t total_cycles;
    uint32_t total_seconds;
} lifecycle_t;

void lifecycle_init(lifecycle_t *lc);

// Records one tool-swap-to-tool-swap use: +1 cycle, + duration_seconds.
void lifecycle_record_use(lifecycle_t *lc, uint32_t duration_seconds);

// True once total_cycles has reached or passed max_cycles - the
// maintenance-due threshold is a config value, not hardcoded here, since
// different tool types (soldering tip vs. gripper jaw) wear out at very
// different cycle counts.
bool lifecycle_needs_maintenance(const lifecycle_t *lc, uint32_t max_cycles);

#endif // LIFECYCLE_H
