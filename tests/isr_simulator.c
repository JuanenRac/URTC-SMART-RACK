// =============================================================================
// URTC-SMART-RACK Firmware - interrupt simulator for the serial receive path: isr_simulator.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// A host program with its own main(). The receive interrupt of the rack does not exist yet (no board, no transport), so this plays it with the thing that is
// closest to one on a host: a POSIX signal. A timer delivers SIGALRM many times a second; the handler does what the real interrupt handler is allowed to do,
// which is call rack_rx_on_byte() with the next byte of a stream, and nothing else. The main loop meanwhile runs rack_rx_poll(), applies the decisions to the
// inventory and runs the tick of the link, so the handler preempts it at any instruction, in the middle of a ring operation included, as an interrupt
// preempts the main loop on the chip.
//
// What it checks: with a handler that keeps pace the main loop gets the decision of every good frame, in order for each tool, with no byte lost, the targets
// of the inventory end where the last good frame left them and the damaged frames are counted by the deframer; with a handler that floods a slow main loop the
// ring overflows and says so, and still no decision ever carries a target that no frame of the stream carried. The deframer, the sequence numbers and the link
// watchdog are only ever touched by the main loop, so they cannot be seen half updated.
//
// Linux only: elsewhere the program says it was skipped and succeeds.
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>

#include "test_runner.h"

#if defined(__linux__)

#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "../src/protocol.h"
#include "../src/rack_app.h"
#include "../src/rack_command.h"
#include "../src/rack_rx.h"
#include "../src/tool_limits.h"

#define FRAMES 200u
#define STREAM_MAX (FRAMES * (PROTOCOL_MAX_FRAME_SIZE + 3u))

static uint8_t stream[STREAM_MAX];
static unsigned stream_len;
static volatile unsigned stream_pos;        // written only by the handler
static volatile unsigned burst_max = 1u;    // bytes the handler delivers per signal
static unsigned lcg = 4242u;
static rack_rx_t rx;

static unsigned next_random(void)
{
    lcg = lcg * 1103515245u + 12345u;
    return (lcg >> 16) & 0x7FFFu;
}

// The "interrupt": the only thing it does is put bytes in the ring.
static void on_alarm(int signo)
{
    (void)signo;
    unsigned n = 1u + next_random() % burst_max;
    while (n-- && stream_pos < stream_len) {
        (void)rack_rx_on_byte(&rx, stream[stream_pos]);
        stream_pos = stream_pos + 1u;
    }
}

static uint8_t tool_of(unsigned n) { return (n % 2u) == 0u ? TOOL_ID_SOLDERING_IRON : TOOL_ID_3D_PRINTER; }
static uint16_t temp_of(unsigned n) { return (uint16_t)(100u + n % 100u); }

// FRAMES commands, the tools alternating, the sequence number rising with the frame number, a little noise between some of them and a bad CRC on every
// twentieth frame. Returns the number of good frames.
static unsigned build_stream(void)
{
    unsigned pos = 0u, good = 0u;
    uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
    for (unsigned n = 0u; n < FRAMES; n++) {
        if (n % 7u == 3u) {
            stream[pos++] = 0x00u;
            stream[pos++] = (uint8_t)(0x40u + n % 5u);
        }
        const uint16_t temp = temp_of(n);
        const uint8_t payload[3] = {tool_of(n), (uint8_t)(temp >> 8), (uint8_t)(temp & 0xFFu)};
        const uint8_t len = protocol_encode_frame((uint8_t)n, RACK_CMD_SET_PREHEAT, payload, 3, buf);
        if (n % 20u == 19u) {
            buf[len - 1u] ^= 0x55u;                 // a damaged frame: it must be counted and must never become a target
        } else {
            good++;
        }
        memcpy(&stream[pos], buf, len);
        pos += len;
    }
    stream_len = pos;
    return good;
}

static uint32_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((unsigned)ts.tv_sec * 1000u + (unsigned)ts.tv_nsec / 1000000u);
}

typedef struct {
    unsigned applied;
    int last_seq[2];
    unsigned out_of_order;
    unsigned impossible;
    uint16_t final_target[2];
    uint32_t crc_errors;
    bool link_lost_seen;
} outcome_t;

static uint16_t target_of(const rack_inventory_t *inv, uint8_t tool)
{
    uint8_t slot;
    rack_slot_t s;
    return rack_inventory_find_tool(inv, tool, &slot) && rack_inventory_get_slot(inv, slot, &s) ? s.target_temp_c : 0xFFFFu;
}

// Runs one round: starts the timer, polls until the whole stream was delivered and the ring is empty, stops the timer.
static outcome_t play(unsigned burst, unsigned pause_us, long timer_us)
{
    outcome_t out = {0u, {-1, -1}, 0u, 0u, {0u, 0u}, 0u, false};
    rack_inventory_t inv;
    link_watchdog_t lw;
    rack_inventory_init(&inv);
    (void)rack_inventory_set_slot_tool(&inv, 0, TOOL_ID_SOLDERING_IRON);
    (void)rack_inventory_set_slot_tool(&inv, 1, TOOL_ID_3D_PRINTER);
    link_watchdog_init(&lw, 5000u);
    link_watchdog_set_command_interval(&lw, 0u);    // every command is applied: the simulator checks the receive path, not the limiter
    rack_rx_init(&rx, 0);
    stream_pos = 0u;
    burst_max = burst;

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_alarm;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);
    struct itimerval timer = {{0, timer_us}, {0, timer_us}};
    setitimer(ITIMER_REAL, &timer, NULL);

    for (;;) {
        const bool delivered = stream_pos >= stream_len;     // read BEFORE the poll: a poll that finds nothing after this drained everything
        const uint32_t now = now_ms();
        rack_decision_t d;
        bool any = false;
        while (rack_rx_poll(&rx, &lw, now, &d)) {
            any = true;
            if (d.action == RACK_ACT_SET_TARGET) {
                const int side = d.tool_id == TOOL_ID_SOLDERING_IRON ? 0 : 1;
                const unsigned n = d.reply_seq;
                if (d.tool_id != tool_of(n) || d.temp_c != temp_of(n) || n % 20u == 19u) {
                    out.impossible++;                       // a target no good frame of the stream carried
                } else if ((int)n <= out.last_seq[side]) {
                    out.out_of_order++;
                }
                out.last_seq[side] = (int)n;
                out.applied++;
            }
            (void)rack_app_apply_decision(&inv, &d);
        }
        if (out.applied > 0u && rack_app_tick(&inv, &lw, now)) {
            out.link_lost_seen = true;                      // the link is alive for the whole run: it must never be reported lost
        }
        if (!any && delivered) {
            break;
        }
        if (pause_us != 0u) {
            // a slow main loop: busy for that long (a nap would be cut short by every signal, which would hide the overflow)
            struct timespec t0, t1;
            clock_gettime(CLOCK_MONOTONIC, &t0);
            do {
                clock_gettime(CLOCK_MONOTONIC, &t1);
            } while ((t1.tv_sec - t0.tv_sec) * 1000000L + (t1.tv_nsec - t0.tv_nsec) / 1000L < (long)pause_us);
        }
    }
    struct itimerval stop = {{0, 0}, {0, 0}};
    setitimer(ITIMER_REAL, &stop, NULL);
    out.final_target[0] = target_of(&inv, TOOL_ID_SOLDERING_IRON);
    out.final_target[1] = target_of(&inv, TOOL_ID_3D_PRINTER);
    out.crc_errors = rx.deframer.stats.crc_errors;
    return out;
}

int main(void)
{
    int failures_value = 0;
    int *failures = &failures_value;
    const unsigned good = build_stream();

    // A handler that keeps pace (at most two bytes per signal against a main loop that never sleeps): every good frame, in order for each tool, no byte lost.
    outcome_t paced = play(2u, 0u, 200);
    TEST_ASSERT(paced.applied == good, "a handler that keeps pace: every good frame of the stream becomes a target");
    TEST_ASSERT(paced.out_of_order == 0u && paced.impossible == 0u, "in order for each tool, and never a target that no frame carried");
    TEST_ASSERT(rx.overflow_bytes == 0u, "and the ring never lost a byte");
    TEST_ASSERT(paced.crc_errors >= FRAMES / 20u - 1u, "the damaged frames were counted by the deframer, in the main loop");
    TEST_ASSERT(!paced.link_lost_seen, "a link that delivers frames is never reported lost");
    // frame 198 is the last of the iron (even), 199 the last of the hotend (odd) and the damaged one (every twentieth: 19, 39, ..., 199) is 199: the hotend ends at 197
    TEST_ASSERT(paced.final_target[0] == temp_of(198u) && paced.final_target[1] == temp_of(197u), "the targets of the inventory end where the last good frame of each tool left them");

    // A flood (up to 120 bytes per signal, more than the ring holds) against a main loop that naps two milliseconds between passes: the ring overflows and says so, and no target is invented.
    outcome_t flood = play(120u, 2000u, 300);
    TEST_ASSERT(flood.impossible == 0u && flood.out_of_order == 0u, "a flood never produces an invented or a repeated target");
    TEST_ASSERT(rx.overflow_bytes > 0u, "the flood overflows the ring and the receiver counts the bytes it lost");
    TEST_ASSERT(flood.applied < good, "so commands are lost, not guessed");

    if (failures_value == 0) {
        printf("All tests passed.\n");
        return 0;
    }
    printf("%d test(s) failed.\n", failures_value);
    return 1;
}

#else

int main(void)
{
    printf("SKIP: the interrupt simulator needs POSIX signals (Linux).\n");
    return 0;
}

#endif
