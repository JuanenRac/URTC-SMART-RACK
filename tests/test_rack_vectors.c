// =============================================================================
// URTC-SMART-RACK Firmware - Host-side tests: the shared vectors (test_rack_vectors.c)
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// tests/vectors/rack_vectors.h is written by URTC-COMMON from its own independent model of the rack (the one the tester and the web
// studio use to simulate a rack). Every frame, every command check, every CAN form, every link scenario and every thermal slot
// verdict in it is reproduced here by the real code of the rack, so the model and the firmware cannot drift apart unnoticed.
#include <math.h>
#include <string.h>

#include "../src/protocol.h"
#include "../src/rack_can.h"
#include "../src/rack_command.h"
#include "../src/rack_link.h"
#include "../src/link_watchdog.h"
#include "../src/slot_verify.h"
#include "test_runner.h"
#include "vectors/rack_vectors.h"

void run_rack_vectors_tests(int *failures)
{
    for (int i = 0; i < RACK_VEC_COUNT(RACK_VEC_FRAMES); i++) {
        const rack_vec_frame_t *v = &RACK_VEC_FRAMES[i];
        uint8_t out[PROTOCOL_MAX_FRAME_SIZE];
        const uint8_t n = protocol_encode_frame(v->seq, v->cmd, v->payload, v->payload_len, out);
        TEST_ASSERT(n == v->frame_len && memcmp(out, v->frame, v->frame_len) == 0, v->name);
        protocol_frame_t parsed;
        TEST_ASSERT(protocol_parse_frame(v->frame, v->frame_len, &parsed) == PROTOCOL_OK && parsed.seq == v->seq &&
                    parsed.cmd == v->cmd && parsed.len == v->payload_len && memcmp(parsed.payload, v->payload, v->payload_len) == 0,
                    "a vector frame parses back to its fields");
    }
    for (int i = 0; i < RACK_VEC_COUNT(RACK_VEC_PARSE); i++) {
        const rack_vec_parse_t *v = &RACK_VEC_PARSE[i];
        protocol_frame_t parsed;
        TEST_ASSERT((int)protocol_parse_frame(v->buf, v->len, &parsed) == v->status, v->name);
    }
    for (int i = 0; i < RACK_VEC_COUNT(RACK_VEC_VALIDATE); i++) {
        const rack_vec_validate_t *v = &RACK_VEC_VALIDATE[i];
        rack_set_preheat_t cmd;
        const rack_command_status_t st = rack_command_validate_set_preheat(v->payload, v->len, &cmd);
        TEST_ASSERT((int)st == v->status, v->name);
        if (st == RACK_CMD_OK) {
            TEST_ASSERT((int)cmd.tool_id == v->tool && (int)cmd.target_temp_c == v->target, v->name);
        }
    }
    for (int i = 0; i < RACK_VEC_COUNT(RACK_VEC_CAN); i++) {
        const rack_vec_can_t *v = &RACK_VEC_CAN[i];
        uint8_t data[8];
        TEST_ASSERT(rack_can_encode_set_preheat(v->seq, v->tool, v->target, data) == 8u && memcmp(data, v->data, 8) == 0, v->name);
        uint8_t seq = 0;
        rack_set_preheat_t cmd;
        TEST_ASSERT(rack_can_decode_set_preheat(RACK_CAN_ID_SET_PREHEAT, v->data, 8, &seq, &cmd) == RACK_CMD_OK && seq == v->seq &&
                    cmd.tool_id == v->tool && cmd.target_temp_c == v->target, "a vector CAN frame decodes to its fields");
    }
    link_watchdog_t lw;
    int current = -1;
    for (int i = 0; i < RACK_VEC_COUNT(RACK_VEC_STEPS); i++) {
        const rack_vec_step_t *v = &RACK_VEC_STEPS[i];
        if (v->scenario != current) {
            link_watchdog_init(&lw, LINK_WATCHDOG_DEFAULT_TIMEOUT_MS);
            current = v->scenario;
        }
        rack_decision_t d;
        rack_link_decide(&lw, v->buf, v->len, v->time_ms, &d);
        TEST_ASSERT((int)d.action == v->action && (int)d.acknowledge == v->ack, v->name);
        if (v->action != 0) {
            TEST_ASSERT((int)d.tool_id == v->tool && (int)d.temp_c == v->temp, v->name);
        }
    }
    for (int i = 0; i < RACK_VEC_COUNT(RACK_VEC_VERIFY); i++) {
        const rack_vec_verify_t *v = &RACK_VEC_VERIFY[i];
        TEST_ASSERT((int)slot_verify(v->occupied != 0, (uint16_t)v->commanded_c, v->measured_c, v->ambient_c, v->elapsed_ms) == v->verdict, v->name);
    }
}
