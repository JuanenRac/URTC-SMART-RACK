// =============================================================================
// URTC-SMART-RACK Firmware - Smart Idle pre-heat logic: preheat.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
// The README's "Smart Idle" workflow (see the SMART RACK WORKFLOW diagram:
// "PRE-HEAT: Soldering Tip to 200 C"): deciding *whether* to pre-heat and
// *what temperature* to target is a pure scheduling/lookup decision, not
// an actual heater PWM/ADC control loop - real and testable without the
// PCB that would carry a real heating element.
#ifndef PREHEAT_H
#define PREHEAT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TOOL_TYPE_SOLDERING_IRON,
    TOOL_TYPE_HOT_AIR,
    TOOL_TYPE_GENERIC, // Grippers, no-contact tools, anything with no useful preheat target.
} tool_type_t;

// Target temperature in whole degrees Celsius for a tool type - 0 means
// "this tool type has nothing to preheat" (TOOL_TYPE_GENERIC).
uint16_t preheat_target_temp_c(tool_type_t type);

// True when an anticipated tool swap is close enough that pre-heating
// should start now, but hasn't already happened (ms_until_next_use == 0
// means the swap is either happening right now or there's no anticipated
// swap at all - either way, "start pre-heating" no longer applies).
bool preheat_should_activate(uint32_t ms_until_next_use, uint32_t lead_time_ms);

// The real, safe target when the host link is lost or was never
// established (see link_watchdog.h's link_watchdog_is_link_lost()) -
// always "off", regardless of tool type or whatever target was last
// commanded. The promotion audit's own "estado seguro al arrancar o
// perder enlace": a lost link must never leave a heater running
// unattended.
uint16_t preheat_safe_state_temp_c(void);

#endif // PREHEAT_H
