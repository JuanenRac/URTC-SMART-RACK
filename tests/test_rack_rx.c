// =============================================================================
// URTC-SMART-RACK Firmware - tests for rack_rx.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// The interrupt-safe serial receive path: the bytes go in one at a time through the call an interrupt would make (rack_rx_on_byte) and the decisions come out
// through the call of the main loop (rack_rx_poll).
#include <string.h>

#include "test_runner.h"
#include "../src/protocol.h"
#include "../src/rack_app.h"
#include "../src/rack_command.h"
#include "../src/rack_rx.h"
#include "../src/tool_limits.h"

static uint8_t encode_preheat(uint8_t seq, uint8_t tool, uint16_t temp, uint8_t *out)
{
    uint8_t payload[3] = {tool, (uint8_t)(temp >> 8), (uint8_t)(temp & 0xFFu)};
    return protocol_encode_frame(seq, RACK_CMD_SET_PREHEAT, payload, 3, out);
}

static void feed(rack_rx_t *rx, const uint8_t *bytes, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++) {
        (void)rack_rx_on_byte(rx, bytes[i]);
    }
}

void run_rack_rx_tests(int *failures)
{
    rack_rx_t rx;
    link_watchdog_t lw;
    rack_decision_t d;
    uint8_t frame[PROTOCOL_MAX_FRAME_SIZE];

    // Argument checks fail closed.
    rack_rx_init(&rx, 0);
    link_watchdog_init(&lw, 1000);
    rack_rx_init(NULL, 0);
    TEST_ASSERT(!rack_rx_on_byte(NULL, 1), "a NULL receiver takes no byte");
    TEST_ASSERT(!rack_rx_poll(NULL, &lw, 0, &d) && !rack_rx_poll(&rx, NULL, 0, &d) && !rack_rx_poll(&rx, &lw, 0, NULL), "poll refuses missing arguments");
    TEST_ASSERT(!rack_rx_poll(&rx, &lw, 0, &d), "an empty ring yields nothing");

    // A frame that arrives one byte at a time gives one decision, and only when its last byte is in.
    uint8_t len = encode_preheat(1, TOOL_ID_SOLDERING_IRON, 200, frame);
    feed(&rx, frame, (uint8_t)(len - 1));
    TEST_ASSERT(!rack_rx_poll(&rx, &lw, 10, &d), "a frame one byte short yields nothing yet");
    feed(&rx, &frame[len - 1], 1);
    TEST_ASSERT(rack_rx_poll(&rx, &lw, 11, &d), "the last byte completes the frame");
    TEST_ASSERT(d.action == RACK_ACT_SET_TARGET && d.tool_id == TOOL_ID_SOLDERING_IRON && d.temp_c == 200 && d.reply == RACK_REPLY_APPLIED, "and the decision is the command that was sent");
    TEST_ASSERT(!rack_rx_poll(&rx, &lw, 12, &d), "nothing is left");

    // Two frames waiting in the ring come out one decision each, in order.
    uint8_t second[PROTOCOL_MAX_FRAME_SIZE];
    const uint8_t second_len = encode_preheat(2, TOOL_ID_3D_PRINTER, 230, second);
    len = encode_preheat(3, TOOL_ID_SOLDERING_IRON, 210, frame);
    feed(&rx, frame, len);
    feed(&rx, second, second_len);
    TEST_ASSERT(rack_rx_poll(&rx, &lw, 100, &d) && d.tool_id == TOOL_ID_SOLDERING_IRON && d.temp_c == 210, "the first of two waiting frames");
    TEST_ASSERT(rack_rx_poll(&rx, &lw, 101, &d) && d.tool_id == TOOL_ID_3D_PRINTER && d.temp_c == 230, "then the second");
    TEST_ASSERT(!rack_rx_poll(&rx, &lw, 102, &d), "then nothing");

    // A damaged frame is not a decision, and the good frame right behind it still is (resynchronisation).
    len = encode_preheat(4, TOOL_ID_SOLDERING_IRON, 220, frame);
    frame[len - 1] ^= 0x55u;
    feed(&rx, frame, len);
    len = encode_preheat(5, TOOL_ID_SOLDERING_IRON, 225, frame);
    feed(&rx, frame, len);
    TEST_ASSERT(rack_rx_poll(&rx, &lw, 200, &d) && d.temp_c == 225, "the good frame behind a bad CRC is read");
    TEST_ASSERT(rx.deframer.stats.crc_errors == 1u && !rack_rx_poll(&rx, &lw, 201, &d), "the bad one was counted and nothing else comes out");

    // A frame hidden inside the bytes of a refused candidate comes out through the second call: one decision, not lost.
    const uint8_t false_header[] = {PROTOCOL_SOF, PROTOCOL_VERSION, 0, 0x7F, PROTOCOL_MAX_PAYLOAD};
    uint8_t hidden[PROTOCOL_MAX_FRAME_SIZE * 2];
    memcpy(hidden, false_header, sizeof false_header);
    const uint8_t good_len = encode_preheat(6, TOOL_ID_3D_PRINTER, 240, frame);
    memcpy(hidden + sizeof false_header, frame, good_len);
    uint32_t hidden_len = sizeof false_header + good_len;
    while (hidden_len < PROTOCOL_MAX_FRAME_SIZE) {
        hidden[hidden_len++] = 0x00;
    }
    feed(&rx, hidden, (uint8_t)hidden_len);
    TEST_ASSERT(rack_rx_poll(&rx, &lw, 300, &d) && d.tool_id == TOOL_ID_3D_PRINTER && d.temp_c == 240, "a frame inside a refused candidate is not lost");
    TEST_ASSERT(!rack_rx_poll(&rx, &lw, 301, &d), "and comes out once");

    // A retransmission is acknowledged as already applied and changes nothing.
    len = encode_preheat(6, TOOL_ID_3D_PRINTER, 240, frame);
    feed(&rx, frame, len);
    TEST_ASSERT(rack_rx_poll(&rx, &lw, 400, &d) && d.action == RACK_ACT_NONE && d.reply == RACK_REPLY_REPEAT, "a retransmission is a repeat, not a new target");

    // A ring that overflows says how many bytes it lost; what is read is still whole frames.
    rack_rx_init(&rx, 0);
    link_watchdog_init(&lw, 1000);
    for (uint8_t i = 0; i < 100; i++) {
        (void)rack_rx_on_byte(&rx, (uint8_t)(0x11u + i));
    }
    (void)rack_rx_poll(&rx, &lw, 500, &d);
    TEST_ASSERT(rx.overflow_bytes == 100u - (RACK_RX_RING_SIZE - 1u), "the bytes the ring could not hold are counted");

    // Interrupt side into the pipeline: the targets of a rack follow the frames and a lost link brings them to the safe state.
    rack_inventory_t inv;
    rack_inventory_init(&inv);
    rack_inventory_set_slot_tool(&inv, 0, TOOL_ID_SOLDERING_IRON);
    rack_rx_init(&rx, 0);
    link_watchdog_init(&lw, 1000);
    len = encode_preheat(1, TOOL_ID_SOLDERING_IRON, 180, frame);
    feed(&rx, frame, len);
    while (rack_rx_poll(&rx, &lw, 600, &d)) {
        (void)rack_app_apply_decision(&inv, &d);
    }
    uint8_t slot;
    rack_slot_t s;
    TEST_ASSERT(rack_inventory_find_tool(&inv, TOOL_ID_SOLDERING_IRON, &slot) && rack_inventory_get_slot(&inv, slot, &s) && s.target_temp_c == 180, "the target reaches the inventory");
    TEST_ASSERT(rack_app_tick(&inv, &lw, 3000) && rack_inventory_get_slot(&inv, slot, &s) && s.target_temp_c == 0, "and a silence brings it to the safe state");
}
