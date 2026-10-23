// GENERATED from spec by tools/make_extra_vectors.py - do not edit by hand.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
#ifndef RACK_VECTORS_H
#define RACK_VECTORS_H

#include <math.h>
#include <stdint.h>

typedef struct { const char *name; uint8_t seq, cmd, payload_len; uint8_t payload[16]; uint8_t frame_len; uint8_t frame[22]; } rack_vec_frame_t;
typedef struct { const char *name; uint8_t len; uint8_t buf[24]; int status; } rack_vec_parse_t; // protocol_status_t order
typedef struct { const char *name; uint8_t len; uint8_t payload[4]; int status; int tool; int target; } rack_vec_validate_t; // rack_command_status_t order
typedef struct { const char *name; uint8_t seq, tool; uint16_t target; uint8_t data[8]; } rack_vec_can_t;
typedef struct { const char *name; int occupied; int commanded_c; float measured_c; float ambient_c; uint32_t elapsed_ms; int verdict; } rack_vec_verify_t; // slot_verdict_t order
typedef struct { int scenario; const char *name; uint32_t time_ms; uint8_t len; uint8_t buf[24]; int action; int tool; int temp; int ack; } rack_vec_step_t; // rack_action_t order

static const rack_vec_frame_t RACK_VEC_FRAMES[] = {
    {"set 200 C on the soldering iron", 1, 1, 3, {0x00, 0x00, 0xC8}, 9, {0xA5, 0x02, 0x01, 0x01, 0x03, 0x00, 0x00, 0xC8, 0x30}},
    {"set 0 on the hot air nozzle", 255, 1, 3, {0x13, 0x00, 0x00}, 9, {0xA5, 0x02, 0xFF, 0x01, 0x03, 0x13, 0x00, 0x00, 0xDF}},
    {"unknown command, empty", 7, 126, 0, {0}, 6, {0xA5, 0x02, 0x07, 0x7E, 0x00, 0x50}},
    {"full payload", 9, 1, 16, {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F}, 22, {0xA5, 0x02, 0x09, 0x01, 0x10, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x9B}},
};

static const rack_vec_parse_t RACK_VEC_PARSE[] = {
    {"good frame", 9, {0xA5, 0x02, 0x03, 0x01, 0x03, 0x00, 0x00, 0xFA, 0xFC}, 0},
    {"short", 5, {0xA5, 0x02, 0x03, 0x01, 0x03}, 1},
    {"frame cut after the header", 7, {0xA5, 0x02, 0x03, 0x01, 0x03, 0x00, 0x00}, 1},
    {"wrong start byte", 9, {0x5A, 0x02, 0x03, 0x01, 0x03, 0x00, 0x00, 0xFA, 0xFC}, 2},
    {"version 1, the little-endian layout", 9, {0xA5, 0x01, 0x03, 0x01, 0x03, 0x00, 0x00, 0xFA, 0xFC}, 3},
    {"length 17", 9, {0xA5, 0x02, 0x03, 0x01, 0x11, 0x00, 0x00, 0xFA, 0xFC}, 4},
    {"bad crc", 9, {0xA5, 0x02, 0x03, 0x01, 0x03, 0x00, 0x00, 0xFA, 0x03}, 5},
};

static const rack_vec_validate_t RACK_VEC_VALIDATE[] = {
    {"soldering 250", 3, {0x00, 0x00, 0xFA}, 0, 0, 250},
    {"soldering 400", 3, {0x00, 0x01, 0x90}, 0, 0, 400},
    {"soldering 401", 3, {0x00, 0x01, 0x91}, 3, -1, -1},
    {"printer 310", 3, {0x0A, 0x01, 0x36}, 0, 10, 310},
    {"printer 311", 3, {0x0A, 0x01, 0x37}, 3, -1, -1},
    {"hot air 400", 3, {0x13, 0x01, 0x90}, 0, 19, 400},
    {"gripper 0 (no heater, target 0)", 3, {0x06, 0x00, 0x00}, 0, 6, 0},
    {"gripper 100", 3, {0x06, 0x00, 0x64}, 5, -1, -1},
    {"no tool (31)", 3, {0x1F, 0x00, 0x00}, 2, -1, -1},
    {"tool 25", 3, {0x19, 0x00, 0x00}, 4, -1, -1},
    {"tool byte 32", 3, {0x20, 0x00, 0x00}, 4, -1, -1},
    {"two bytes", 2, {0x00, 0x01}, 1, -1, -1},
    {"four bytes", 4, {0x00, 0x00, 0x01, 0x00}, 1, -1, -1},
    {"empty", 0, {0}, 1, -1, -1},
};

static const rack_vec_can_t RACK_VEC_CAN[] = {
    {"soldering 250", 4, 0, 250, {0x00, 0x00, 0xFA, 0x04, 0x00, 0x00, 0x00, 0x00}},
    {"hot air 350", 200, 19, 350, {0x13, 0x01, 0x5E, 0xC8, 0x00, 0x00, 0x00, 0x00}},
    {"off", 0, 5, 0, {0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
};

static const rack_vec_step_t RACK_VEC_STEPS[] = {
    {0, "apply, retransmit, stale, new", 0, 9, {0xA5, 0x02, 0x0A, 0x01, 0x03, 0x00, 0x00, 0xC8, 0x04}, 1, 0, 200, 1},
    {0, "apply, retransmit, stale, new", 20, 9, {0xA5, 0x02, 0x0A, 0x01, 0x03, 0x00, 0x00, 0xC8, 0x04}, 0, 0, 0, 1},
    {0, "apply, retransmit, stale, new", 40, 9, {0xA5, 0x02, 0x09, 0x01, 0x03, 0x00, 0x00, 0xFA, 0xE1}, 0, 0, 0, 1},
    {0, "apply, retransmit, stale, new", 60, 9, {0xA5, 0x02, 0x0B, 0x01, 0x03, 0x00, 0x00, 0xFA, 0xB3}, 1, 0, 250, 1},
    {1, "refused command sends the tool to the safe state", 0, 9, {0xA5, 0x02, 0x01, 0x01, 0x03, 0x00, 0x00, 0xC8, 0x30}, 1, 0, 200, 1},
    {1, "refused command sends the tool to the safe state", 10, 9, {0xA5, 0x02, 0x02, 0x01, 0x03, 0x00, 0x01, 0xF4, 0xEA}, 2, 0, 0, 0},
    {1, "refused command sends the tool to the safe state", 20, 9, {0xA5, 0x02, 0x02, 0x01, 0x03, 0x00, 0x00, 0xC8, 0x4B}, 1, 0, 200, 1},
    {2, "silence resets the sequences", 0, 9, {0xA5, 0x02, 0x32, 0x01, 0x03, 0x13, 0x01, 0x2C, 0x56}, 1, 19, 300, 1},
    {2, "silence resets the sequences", 100, 9, {0xA5, 0x02, 0x32, 0x01, 0x03, 0x13, 0x01, 0x2C, 0x56}, 0, 0, 0, 1},
    {2, "silence resets the sequences", 1000, 9, {0xA5, 0x02, 0x32, 0x01, 0x03, 0x13, 0x01, 0x2C, 0x56}, 1, 19, 300, 1},
    {3, "noise changes nothing", 0, 9, {0xA5, 0x02, 0x01, 0x01, 0x03, 0x00, 0x00, 0xC8, 0x30}, 1, 0, 200, 1},
    {3, "noise changes nothing", 10, 3, {0xA5, 0x01, 0x02}, 0, 0, 0, 0},
    {3, "noise changes nothing", 20, 6, {0xA5, 0x02, 0x02, 0x7E, 0x00, 0x90}, 0, 0, 0, 0},
    {3, "noise changes nothing", 30, 9, {0xA5, 0x02, 0x01, 0x01, 0x03, 0x06, 0x00, 0x64, 0x00}, 2, 6, 0, 0},
};

static const rack_vec_verify_t RACK_VEC_VERIFY[] = {
    {"no reading", 1, 200, NAN, 24.0f, 0u, 0},
    {"empty and cold", 0, 0, 25.0f, 24.0f, 0u, 1},
    {"empty with a heater on", 0, 0, 80.0f, 24.0f, 0u, 2},
    {"empty, a little warm", 0, 0, 60.0f, 24.0f, 0u, 1},
    {"idle and cool", 1, 0, 30.0f, 24.0f, 0u, 3},
    {"idle and still hot", 1, 0, 120.0f, 24.0f, 0u, 4},
    {"heating, early", 1, 300, 150.0f, 24.0f, 5000u, 5},
    {"heating, too slow", 1, 300, 150.0f, 24.0f, 60000u, 7},
    {"reached", 1, 300, 290.0f, 24.0f, 20000u, 6},
    {"reached, just inside", 1, 300, 285.0f, 24.0f, 20000u, 6},
    {"just outside, early", 1, 300, 284.0f, 24.0f, 59999u, 5},
    {"overshoot", 1, 300, 330.0f, 24.0f, 90000u, 8},
    {"overshoot edge", 1, 300, 325.0f, 24.0f, 90000u, 6},
};

#define RACK_VEC_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

#endif // RACK_VECTORS_H
