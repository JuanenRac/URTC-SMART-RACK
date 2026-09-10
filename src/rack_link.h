// =============================================================================
// URTC-SMART-RACK Firmware - Real receive-path decision: rack_link.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
// Found while auditing the code: the real
// frame-dispatch decision tying protocol/rack-command/link-watchdog/
// preheat together existed only as a static function inside this repo's
// own tests/test_rack_link_scenarios.c, not as a real src/ module anything
// else could call. Promoted here so the real receive-frame decision is
// ready to wire into a real UART/CAN ISR the day the PCB exists (see
// main.c's own note - there is no confirmed CAN wiring for this board
// yet) - the tests now call INTO this module instead of defining their
// own private copy of the logic under test.
#ifndef RACK_LINK_H
#define RACK_LINK_H

#include <stdint.h>

#include "link_watchdog.h"

// The real decision a receive path makes for one incoming buffer: parse
// it, validate the command if it's a real SET_PREHEAT, and return the
// target temperature that should actually be applied - the requested
// target on success, or the real safe-state temperature
// (preheat_safe_state_temp_c()) for anything that fails at any layer
// (malformed/corrupt frame, a real duplicate resend, an unrelated
// command, or a well-formed but out-of-range command). `lw` is mutated
// exactly as a real receive interrupt handler's own watchdog state would
// be - callers share one `link_watchdog_t` across every real call the
// same way a real ISR would share one across every real frame.
uint16_t rack_link_process_frame(link_watchdog_t *lw, const uint8_t *buf, uint8_t buf_len, uint32_t now_ms);

#endif // RACK_LINK_H
