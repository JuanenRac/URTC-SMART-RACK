// =============================================================================
// URTC-SMART-RACK Firmware - Virtual rack: the real rack logic behind a text port
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// The modules the rack firmware is made of (receive ring, deframer, link decision, watchdog, inventory, pre-heat limit, reply encoders) joined the way the main
// loop of the board joins them, compiled for the host, behind a text port: one command per line on standard input, one line per frame the rack would transmit on
// standard output. A script (tools/rack_bench.py) plays scenarios through it, with faults, and compares the answers with the Python model of the rack.
//
// Input lines (hexadecimal values, no 0x):
//   I <slot> <tool> <serial>   put a tool in a slot (the inventory the sensors would report)
//   T <ms>                     the clock is now <ms> (never goes back); runs the tick of the main loop (watchdog and pre-heat limit)
//   S <bytes>                  bytes arrive on the serial link, one after the other with the main loop polling after each
//   B <bytes>                  the same bytes arrive in one burst before the main loop polls (the receive ring can overflow)
//   C <id> <bytes>             one CAN frame arrives
//   Q                          print the state
//   M <text>                   print the line  M <text>  (a marker the script uses to tell where the answer to one command ends)
// Output lines:
//   S <bytes>                  a serial reply frame
//   C <id> <bytes>             a CAN reply frame
//   Q <slot> <occupied> <tool> <serial> <target> <expired>   one per slot, then   Q link <lost>   and   Q dropped <bytes lost by the ring>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/preheat_limit.h"
#include "../src/protocol.h"
#include "../src/rack_app.h"
#include "../src/rack_can.h"
#include "../src/rack_inventory.h"
#include "../src/rack_main_loop.h"
#include "../src/rack_reply.h"
#include "../src/rack_rx.h"

#define LINE_MAX_CHARS 4096

static rack_ctx_t ctx;
#define inventory (ctx.inventory)
#define watchdog (ctx.watchdog)
#define limit (ctx.limit)
#define rx (ctx.rx)
#define now_ms (ctx.now_ms)

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

// Reads whitespace-separated hexadecimal bytes; returns how many (or -1 for a character that is not hexadecimal or an odd number of digits).
static int parse_bytes(const char *text, uint8_t *out, int max)
{
    int count = 0;
    while (*text != '\0') {
        if (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
            text++;
            continue;
        }
        const int high = hex_digit(text[0]);
        const int low = high < 0 ? -1 : hex_digit(text[1]);
        if (high < 0 || low < 0 || count >= max) {
            return -1;
        }
        out[count++] = (uint8_t)(high * 16 + low);
        text += 2;
    }
    return count;
}

static void print_bytes(const uint8_t *bytes, uint8_t length)
{
    for (uint8_t i = 0; i < length; i++) {
        printf("%02x", (unsigned)bytes[i]);
    }
}

static void answer_serial(const rack_decision_t *decision)
{
    uint8_t frame[PROTOCOL_MAX_FRAME_SIZE];
    const uint8_t length = rack_reply_encode(decision, frame);
    if (length != 0u) {
        printf("S ");
        print_bytes(frame, length);
        printf("\n");
    }
}

static void answer_can(const rack_decision_t *decision)
{
    uint8_t data[RACK_CAN_REPLY_DLC];
    const uint8_t length = rack_can_encode_reply(decision, data);
    if (length != 0u) {
        printf("C %x ", (unsigned)RACK_CAN_ID_REPLY);
        print_bytes(data, length);
        printf("\n");
    }
}

static void on_reply(const rack_decision_t *decision, bool from_can, void *user)
{
    (void)user;
    if (from_can) {
        answer_can(decision);
    } else {
        answer_serial(decision);
    }
}

static void poll_serial(void)
{
    (void)rack_main_poll_serial(&ctx, on_reply, NULL);
}

static void print_state(void)
{
    for (uint8_t slot = 0; slot < RACK_INVENTORY_MAX_SLOTS; slot++) {
        rack_slot_t entry;
        if (!rack_inventory_get_slot(&inventory, slot, &entry)) {
            continue;
        }
        printf("Q %u %u %u %u %u %u\n", (unsigned)slot, entry.occupied ? 1u : 0u, entry.occupied ? (unsigned)entry.tool_id : 31u, entry.occupied ? (unsigned)entry.serial : 0u,
               (unsigned)entry.target_temp_c, preheat_limit_is_expired(&limit, slot) ? 1u : 0u);
    }
    printf("Q link %u\n", link_watchdog_is_link_lost(&watchdog, now_ms) ? 1u : 0u);
    rx.overflow_bytes += rack_ring_take_dropped(&rx.ring);
    printf("Q dropped %u\n", (unsigned)rx.overflow_bytes);
}

static bool handle(char *line)
{
    uint8_t bytes[LINE_MAX_CHARS / 2];
    unsigned first = 0, second = 0, third = 0;
    switch (line[0]) {
        case 'I':
            if (sscanf(line + 1, "%u %u %u", &first, &second, &third) == 3 && first < RACK_INVENTORY_MAX_SLOTS && second < 256u && third < 256u) {
                (void)rack_inventory_set_slot_unit(&inventory, (uint8_t)first, (uint8_t)second, (uint8_t)third);
                return true;
            }
            return false;
        case 'T':
            if (sscanf(line + 1, "%u", &first) == 1 && rack_main_set_time(&ctx, first)) {
                (void)rack_main_tick(&ctx);
                return true;
            }
            return false;
        case 'S':
        case 'B': {
            const int count = parse_bytes(line + 1, bytes, (int)sizeof bytes);
            if (count < 0) {
                return false;
            }
            for (int i = 0; i < count; i++) {
                (void)rack_rx_on_byte(&rx, bytes[i]);
                if (line[0] == 'S') {
                    poll_serial();
                }
            }
            poll_serial();
            return true;
        }
        case 'C': {
            char *rest = NULL;
            unsigned can_id = 0;
            if (sscanf(line + 1, "%x", &can_id) != 1) {
                return false;
            }
            rest = line + 1;
            while (*rest == ' ') {
                rest++;                                  // the blanks, then the identifier, then the blanks, then the data
            }
            while (*rest != '\0' && *rest != ' ') {
                rest++;
            }
            const int count = parse_bytes(rest, bytes, 8);
            if (count < 0) {
                return false;
            }
            rack_main_on_can(&ctx, can_id, bytes, (uint8_t)count, on_reply, NULL);
            return true;
        }
        case 'M':
            printf("M %s", line + 2);
            return true;
        case 'Q':
            print_state();
            return true;
        default:
            return false;
    }
}

int main(void)
{
    rack_main_init(&ctx);
    char line[LINE_MAX_CHARS];
    while (fgets(line, sizeof line, stdin) != NULL) {
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '#') {
            continue;
        }
        if (!handle(line)) {
            printf("E %s", line);
            if (strchr(line, '\n') == NULL) {
                printf("\n");
            }
        }
        (void)fflush(stdout);
    }
    return 0;
}
