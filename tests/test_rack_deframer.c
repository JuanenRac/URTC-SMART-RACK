// =============================================================================
// URTC-SMART-RACK Firmware - tests for rack_deframer.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// The serial link delivers bytes, not frames: a lost byte, a frame in two pieces, noise between two
// frames and a half frame that never ends are played through the deframer, and then through the
// whole receive path down to the decision about the heater.
#include <string.h>

#include "test_runner.h"
#include "../src/protocol.h"
#include "../src/rack_command.h"
#include "../src/rack_deframer.h"
#include "../src/rack_link.h"

static uint8_t encode_preheat(uint8_t seq, uint8_t tool, uint16_t temp, uint8_t *out)
{
    uint8_t payload[3] = {tool, (uint8_t)(temp & 0xFFu), (uint8_t)(temp >> 8)};
    return protocol_encode_frame(seq, RACK_CMD_SET_PREHEAT, payload, 3, out);
}

// Feeds `n` bytes one per millisecond starting at *clock; counts the frames that come out and
// keeps the last one.
static int feed(rack_deframer_t *d, const uint8_t *bytes, uint32_t n, uint32_t *clock, uint8_t *last, uint8_t *last_len)
{
    int frames = 0;
    uint8_t out[PROTOCOL_MAX_FRAME_SIZE];
    for (uint32_t i = 0; i < n; i++) {
        uint8_t len = rack_deframer_push(d, bytes[i], (*clock)++, out);
        while (len != 0u) {
            frames++;
            memcpy(last, out, len);
            *last_len = len;
            len = rack_deframer_next(d, out);
        }
    }
    return frames;
}

void run_rack_deframer_tests(int *failures)
{
    uint8_t good[PROTOCOL_MAX_FRAME_SIZE];
    const uint8_t good_len = encode_preheat(1, 0, 200, good);
    uint8_t last[PROTOCOL_MAX_FRAME_SIZE];
    uint8_t last_len = 0;
    uint32_t clock = 0;
    rack_deframer_t d;

    // A whole frame, byte by byte, comes out exactly once and is the frame that went in.
    rack_deframer_init(&d, 0);
    TEST_ASSERT(d.timeout_ms == RACK_DEFRAMER_DEFAULT_TIMEOUT_MS, "a timeout of 0 is replaced by the default");
    TEST_ASSERT(feed(&d, good, good_len, &clock, last, &last_len) == 1 && last_len == good_len && memcmp(last, good, good_len) == 0, "a frame fed byte by byte is delivered once and unchanged");
    TEST_ASSERT(d.stats.frames_ok == 1 && d.stats.dropped_bytes == 0 && d.stats.resyncs == 0, "a clean frame costs no resync and no dropped byte");

    // Two frames back to back are two frames.
    uint8_t two[2 * PROTOCOL_MAX_FRAME_SIZE];
    uint8_t second[PROTOCOL_MAX_FRAME_SIZE];
    const uint8_t second_len = encode_preheat(2, 10, 250, second);
    memcpy(two, good, good_len);
    memcpy(two + good_len, second, second_len);
    rack_deframer_init(&d, 0);
    TEST_ASSERT(feed(&d, two, (uint32_t)(good_len + second_len), &clock, last, &last_len) == 2 && memcmp(last, second, second_len) == 0, "two frames back to back are both delivered");

    // A frame that arrives in two bursts with a short pause between them is still one frame.
    rack_deframer_init(&d, 20);
    uint8_t out[PROTOCOL_MAX_FRAME_SIZE];
    uint32_t t = 1000;
    uint8_t produced = 0;
    for (uint8_t i = 0; i < good_len; i++) {
        t += (i == 4u) ? 15u : 1u; // a pause of 15 ms, below the 20 ms limit
        produced = rack_deframer_push(&d, good[i], t, out);
    }
    TEST_ASSERT(produced == good_len && memcmp(out, good, good_len) == 0, "a pause shorter than the timeout does not split the frame");

    // A pause longer than the timeout drops the half frame, and the next whole frame is not lost with it.
    rack_deframer_init(&d, 20);
    t = 5000;
    for (uint8_t i = 0; i < 5u; i++) {
        (void)rack_deframer_push(&d, good[i], t++, out);
    }
    t += 100; // the rest never comes in time
    produced = 0;
    for (uint8_t i = 0; i < good_len; i++) {
        produced = rack_deframer_push(&d, good[i], t++, out);
    }
    TEST_ASSERT(produced == good_len && d.stats.timeouts == 1, "after a stalled half frame the next frame is delivered and the stall is counted");

    // The timeout works across the wrap of the millisecond counter.
    rack_deframer_init(&d, 20);
    t = 0xFFFFFFF0u;
    for (uint8_t i = 0; i < good_len; i++) {
        produced = rack_deframer_push(&d, good[i], t++, out); // wraps to 0 in the middle of the frame
    }
    TEST_ASSERT(produced == good_len && d.stats.timeouts == 0, "a frame that spans the wrap of the clock is not mistaken for a stall");

    // Noise before a frame is skipped.
    rack_deframer_init(&d, 0);
    clock = 0;
    const uint8_t noise[] = {0x00, 0x13, 0xFF, 0x7E, 0x01};
    TEST_ASSERT(feed(&d, noise, sizeof noise, &clock, last, &last_len) == 0 && d.stats.dropped_bytes == sizeof noise, "bytes that are not a start byte are dropped");
    TEST_ASSERT(feed(&d, good, good_len, &clock, last, &last_len) == 1, "the frame after the noise is delivered");

    // A lost byte inside a frame: the damaged frame is refused and the NEXT frame still arrives.
    uint8_t damaged[2 * PROTOCOL_MAX_FRAME_SIZE];
    memcpy(damaged, good, good_len);
    memmove(damaged + 6, damaged + 7, (size_t)(good_len - 7)); // remove the 7th byte of the first frame
    memcpy(damaged + good_len - 1, second, second_len);
    rack_deframer_init(&d, 0);
    clock = 0;
    TEST_ASSERT(feed(&d, damaged, (uint32_t)(good_len - 1 + second_len), &clock, last, &last_len) == 1 && memcmp(last, second, second_len) == 0, "a byte lost inside a frame costs that frame and not the next one");
    TEST_ASSERT(d.stats.crc_errors + d.stats.bad_headers >= 1 && d.stats.resyncs >= 1, "the damaged frame is counted and the deframer resynchronised");

    // A flipped bit in the CRC is refused; the following frame is delivered.
    uint8_t flipped[2 * PROTOCOL_MAX_FRAME_SIZE];
    memcpy(flipped, good, good_len);
    flipped[good_len - 1] ^= 0x40u;
    memcpy(flipped + good_len, second, second_len);
    rack_deframer_init(&d, 0);
    clock = 0;
    TEST_ASSERT(feed(&d, flipped, (uint32_t)(good_len + second_len), &clock, last, &last_len) == 1 && d.stats.crc_errors == 1, "a bad CRC is counted and the next frame still arrives");

    // A start byte inside a payload does not break a good frame.
    uint8_t tricky_payload[3] = {PROTOCOL_SOF, PROTOCOL_SOF, PROTOCOL_VERSION};
    uint8_t tricky[PROTOCOL_MAX_FRAME_SIZE];
    const uint8_t tricky_len = protocol_encode_frame(7, 0x55, tricky_payload, 3, tricky);
    rack_deframer_init(&d, 0);
    clock = 0;
    TEST_ASSERT(feed(&d, tricky, tricky_len, &clock, last, &last_len) == 1 && memcmp(last, tricky, tricky_len) == 0, "start bytes inside a payload do not make the deframer give up on the frame");

    // An impossible length is refused as soon as the header shows it, not after waiting 255 bytes.
    const uint8_t impossible[] = {PROTOCOL_SOF, PROTOCOL_VERSION, 1, RACK_CMD_SET_PREHEAT, 200};
    rack_deframer_init(&d, 0);
    clock = 0;
    TEST_ASSERT(feed(&d, impossible, sizeof impossible, &clock, last, &last_len) == 0 && d.stats.bad_headers == 1 && d.count == 0, "a length above the maximum is refused from the header alone");
    TEST_ASSERT(feed(&d, good, good_len, &clock, last, &last_len) == 1, "and the next frame is delivered");

    // A frame hidden inside the bytes a false candidate was holding is found by rack_deframer_next().
    const uint8_t false_header[] = {PROTOCOL_SOF, PROTOCOL_VERSION, 0, 0x7F, PROTOCOL_MAX_PAYLOAD};
    uint8_t hidden[PROTOCOL_MAX_FRAME_SIZE * 2];
    memcpy(hidden, false_header, sizeof false_header);
    memcpy(hidden + sizeof false_header, good, good_len); // 5 + 9 = 14 bytes so far; the false frame wants 22
    uint32_t hidden_len = sizeof false_header + good_len;
    while (hidden_len < PROTOCOL_MAX_FRAME_SIZE) {
        hidden[hidden_len++] = 0x00; // filler that makes the false candidate complete with a wrong CRC
    }
    rack_deframer_init(&d, 0);
    clock = 0;
    TEST_ASSERT(feed(&d, hidden, hidden_len, &clock, last, &last_len) == 1 && memcmp(last, good, good_len) == 0, "a complete frame inside a refused candidate is not lost");

    // Statistics of a noisy stream: valid frames separated by bursts of pseudo-random bytes are all recovered.
    rack_deframer_init(&d, 0);
    clock = 0;
    uint32_t seed = 12345u;
    int sent = 0;
    int received = 0;
    for (int round = 0; round < 200; round++) {
        uint8_t burst[8];
        const uint32_t burst_len = (seed >> 8) % 9u;
        for (uint32_t i = 0; i < burst_len; i++) {
            seed = seed * 1664525u + 1013904223u;
            burst[i] = (uint8_t)(seed >> 24);
        }
        received += feed(&d, burst, burst_len, &clock, last, &last_len);
        clock += 30; // the line is quiet between bursts and frames: a half frame left by the noise times out
        uint8_t frame[PROTOCOL_MAX_FRAME_SIZE];
        const uint8_t frame_len = encode_preheat((uint8_t)round, (uint8_t)(round % 25), (uint16_t)(round % 300), frame);
        sent++;
        received += feed(&d, frame, frame_len, &clock, last, &last_len);
        clock += 30;
    }
    TEST_ASSERT(received == sent, "every frame of a noisy stream with quiet gaps is recovered");

    // Defensive arguments.
    TEST_ASSERT(rack_deframer_push(NULL, 0, 0, out) == 0 && rack_deframer_push(&d, 0, 0, NULL) == 0, "a missing deframer or buffer is refused");
    TEST_ASSERT(rack_deframer_next(NULL, out) == 0 && rack_deframer_next(&d, NULL) == 0, "next refuses missing arguments");
    rack_deframer_init(NULL, 0);
    rack_deframer_reset(NULL);
    (void)rack_deframer_push(&d, PROTOCOL_SOF, 0, out);
    rack_deframer_reset(&d);
    TEST_ASSERT(d.count == 0, "a reset forgets the bytes held");

    // The whole receive path: bytes in, decision out - and the decision survives a lost byte before it.
    link_watchdog_t lw;
    link_watchdog_init(&lw, 1000);
    rack_deframer_init(&d, 0);
    rack_decision_t decision = {RACK_ACT_NONE, 0, 0, 0};
    bool got = false;
    clock = 100;
    for (uint8_t i = 0; i < good_len; i++) {
        if (i == 3u) {
            continue; // the byte that is lost
        }
        got = rack_link_rx_byte(&d, &lw, good[i], clock++, &decision) || got;
    }
    TEST_ASSERT(!got && decision.action == RACK_ACT_NONE, "a frame with a lost byte reaches no decision");
    uint8_t retry[PROTOCOL_MAX_FRAME_SIZE];
    const uint8_t retry_len = encode_preheat(1, 0, 200, retry);
    for (uint8_t i = 0; i < retry_len; i++) {
        got = rack_link_rx_byte(&d, &lw, retry[i], clock++, &decision);
    }
    TEST_ASSERT(got && decision.action == RACK_ACT_SET_TARGET && decision.tool_id == 0 && decision.temp_c == 200, "the retransmission is turned into a decision by the receive path");
    TEST_ASSERT(!rack_link_rx_next(&d, &lw, clock, &decision), "nothing else is waiting after one frame");
}
