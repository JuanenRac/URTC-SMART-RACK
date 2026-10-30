// =============================================================================
// URTC-SMART-RACK Firmware - Free-configuration heads (ID jumpers 0x1F): free_tool.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// A head whose five ID jumpers all read 1 (0x1F) does not say which tool it is: its firmware takes the tool from a register of its own
// F-RAM (CAN 0x1A2 writes it, 0x1A3 reports it as the raw jumper reading plus "tool id + 1", 0 = not set). The rack therefore never assumes
// "no tool" for such a head, and never guesses one: it ASKS (query 0x1A3), CHECKS the answer and, only then, uses the identity exactly like
// one read from jumpers (the same slot of the inventory, the same limits, the same pre-heat plan).
//
//  * Until a valid answer has arrived the head has no identity: its slot holds no tool, so the command check refuses every target for it
//    (tool absent) and the pre-heat plan orders nothing. A silent or invalid head is the safe state, not a head without a tool.
//  * An answer is valid when it has at least two bytes, its first byte (the jumper reading) is 0x1F and its second one is 1..25 (the tool
//    id is that minus one). 0, 26..255, a short answer or a jumper reading other than 0x1F name no tool. The reading 25..30 of the jumpers
//    themselves is invalid and never asks anything.
//  * One round is up to max_attempts queries, each waiting query_timeout_ms. A round without a valid answer leaves the head invalid for
//    retry_backoff_ms and then starts again, for as long as the head sits in the slot.
//  * A resolved head is asked again every refresh_period_ms (0 = never) to notice a change. The same tool confirms. Another tool is a CHANGE
//    OF TOOL: the slot takes the new tool with no pre-heat target. An invalid answer, or a whole round of silence, withdraws the identity
//    (the slot is emptied) and the head is asked again after the back-off.
//
// All the times live in free_tool_config_t (defaults below), never in the logic. Pure logic: the caller sends the query and feeds the
// answers. The shared vectors of URTC-COMMON (RACK_VEC_FREE_*) are reproduced by tests/test_rack_vectors.c.
//
// Note on the head's firmware: it reads its F-RAM register once, at boot, so a change written with 0x1A2 reaches the answer of 0x1A3 only
// after the head's next power cycle; the refresh therefore notices a change made by swapping the head, or after that restart.
#ifndef FREE_TOOL_H
#define FREE_TOOL_H

#include <stdbool.h>
#include <stdint.h>

#include "rack_inventory.h"

#define FREE_TOOL_CAN_ID_QUERY 0x1A3u         // the query (payload ignored) and its answer
#define FREE_TOOL_JUMPER_READING 0x1Fu        // the jumper reading of a free-configuration head
#define FREE_TOOL_SELECTION_MAX 25u           // the answer carries the tool id plus one
#define FREE_TOOL_REPLY_MIN_LEN 2u

// What the configuration starts with (free_tool_config_default()).
#define FREE_TOOL_DEFAULT_QUERY_TIMEOUT_MS 250u
#define FREE_TOOL_DEFAULT_MAX_ATTEMPTS 3u
#define FREE_TOOL_DEFAULT_RETRY_BACKOFF_MS 2000u
#define FREE_TOOL_DEFAULT_REFRESH_PERIOD_MS 5000u

typedef struct {
    uint32_t query_timeout_ms;   // how long one query waits for the answer
    uint8_t max_attempts;        // queries of one round, the first one included (0 counts as 1)
    uint32_t retry_backoff_ms;   // pause after a round without a valid answer
    uint32_t refresh_period_ms;  // while resolved, ask again this often; 0 = never
} free_tool_config_t;

free_tool_config_t free_tool_config_default(void);

// What a jumper reading means. The order is the one of the shared vectors.
typedef enum { FREE_TOOL_KIND_DIRECT = 0, FREE_TOOL_KIND_FREE = 1, FREE_TOOL_KIND_INVALID = 2 } free_tool_kind_t;

// Bits above the fifth are ignored. DIRECT: 0..24 is a tool (written to *tool_out when it is not NULL); FREE: 31; INVALID: 25..30.
free_tool_kind_t free_tool_classify_jumpers(uint8_t raw_bits, uint8_t *tool_out);

// The tool an answer of 0x1A3 names, or false when it names none (see above). Extra bytes are ignored.
bool free_tool_parse_reply(const uint8_t *data, uint8_t len, uint8_t *tool_out);

// What happened to the identity of the head in a step. The order is the one of the shared vectors.
typedef enum {
    FREE_TOOL_EVT_NONE = 0,
    FREE_TOOL_EVT_RESOLVED = 1,   // first valid answer: the head now has an identity
    FREE_TOOL_EVT_CONFIRMED = 2,  // the same tool again
    FREE_TOOL_EVT_CHANGED = 3,    // another tool: a change of tool
    FREE_TOOL_EVT_LOST = 4        // invalid answer or a whole round of silence: the identity is withdrawn
} free_tool_event_t;

// The state of one head. Treat as opaque; zero it with free_tool_reset().
typedef struct {
    bool resolved;
    uint8_t tool;
    uint8_t attempts;     // queries sent in the round in flight; 0 = none in flight
    bool armed;           // false until the first query has been sent
    uint32_t deadline_ms; // in flight: when the current query gives up
    uint32_t next_ms;     // idle: when the next round starts
} free_tool_t;

void free_tool_reset(free_tool_t *ft);

// One step for one head: hands in the answer when there is one (reply NULL = none; a non-NULL reply of length 0 is an empty answer) and
// returns true when the caller must send the query now. *event (may be NULL) is what happened to the identity: the event of the answer if
// it had one, otherwise the one of the clock.
bool free_tool_step(free_tool_t *ft, const free_tool_config_t *config, uint32_t now_ms, const uint8_t *reply, uint8_t reply_len,
                    free_tool_event_t *event);

// ---- the heads of the slots of the rack ----------------------------------------------------------------------------------------------

typedef enum {
    FREE_TOOL_ID_EMPTY = 0,    // nothing known about the slot
    FREE_TOOL_ID_TOOL = 1,     // the slot holds a tool of known identity (from jumpers or from a valid answer)
    FREE_TOOL_ID_PENDING = 2,  // a free-configuration head that has not given a valid answer: no identity, safe state
    FREE_TOOL_ID_INVALID = 3   // jumpers that read an unassigned value: no identity, safe state
} free_tool_identity_t;

typedef struct {
    free_tool_t head[RACK_INVENTORY_MAX_SLOTS];
    bool free_mode[RACK_INVENTORY_MAX_SLOTS];
    bool invalid[RACK_INVENTORY_MAX_SLOTS];
} free_tool_bank_t;

void free_tool_bank_init(free_tool_bank_t *bank);

// A head sits in `slot` and its jumpers read `raw_bits`. A direct reading racks the tool at once; a free one empties the slot and waits for
// the answer of the head; an invalid one empties the slot for good. Returns the kind (INVALID for a missing bank or inventory or a slot
// out of range, and nothing is changed).
free_tool_kind_t free_tool_bank_set_jumpers(free_tool_bank_t *bank, rack_inventory_t *inv, uint8_t slot, uint8_t raw_bits);

// A step for the head of `slot` (see free_tool_step). A new or changed identity racks the tool in the slot with no target; a withdrawn one
// empties the slot. Does nothing (false) for a slot that is not in free mode.
bool free_tool_bank_step(free_tool_bank_t *bank, const free_tool_config_t *config, rack_inventory_t *inv, uint8_t slot, uint32_t now_ms,
                         const uint8_t *reply, uint8_t reply_len, free_tool_event_t *event);

// Whether the slot holds a known tool, waits for one, or has jumpers that name none.
free_tool_identity_t free_tool_bank_identity(const free_tool_bank_t *bank, const rack_inventory_t *inv, uint8_t slot);

#endif // FREE_TOOL_H
