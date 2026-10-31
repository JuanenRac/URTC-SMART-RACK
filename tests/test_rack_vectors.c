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
#include "../src/free_tool.h"
#include "../src/rack_can.h"
#include "../src/rack_command.h"
#include "../src/rack_deframer.h"
#include "../src/rack_reply.h"
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
    // The reply on CAN (0x261): the three bytes the model of URTC-COMMON writes for each of the nine reasons, and the frames that are not a reply.
    for (int i = 0; i < RACK_VEC_COUNT(RACK_VEC_CAN_REPLIES); i++) {
        const rack_vec_can_reply_t *v = &RACK_VEC_CAN_REPLIES[i];
        rack_decision_t decision;
        memset(&decision, 0, sizeof decision);
        decision.reply = v->code;
        decision.reply_cmd = v->cmd;
        decision.reply_tool = v->tool;
        uint8_t data[RACK_CAN_REPLY_DLC];
        TEST_ASSERT(v->id == RACK_CAN_ID_REPLY && rack_can_encode_reply(&decision, data) == RACK_CAN_REPLY_DLC && memcmp(data, v->data, RACK_CAN_REPLY_DLC) == 0, v->name);
        rack_can_reply_t parsed;
        TEST_ASSERT(rack_can_parse_reply(v->id, v->data, RACK_CAN_REPLY_DLC, &parsed) && parsed.ack == (v->ack != 0) && parsed.cmd == v->cmd &&
                    parsed.tool_id == v->tool && parsed.code == v->code, "a vector CAN reply parses back to its fields");
    }
    for (int i = 0; i < RACK_VEC_COUNT(RACK_VEC_CAN_REPLIES_BAD); i++) {
        const rack_vec_can_reply_bad_t *v = &RACK_VEC_CAN_REPLIES_BAD[i];
        rack_can_reply_t parsed;
        TEST_ASSERT(!rack_can_parse_reply(v->id, v->data, v->len, &parsed), v->name);
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
        // The answer to the sender, byte for byte as the model of URTC-COMMON writes it (length 0: nobody is told).
        uint8_t reply[PROTOCOL_MAX_FRAME_SIZE];
        const uint8_t reply_len = rack_reply_encode(&d, reply);
        TEST_ASSERT(reply_len == v->reply_len && memcmp(reply, v->reply, v->reply_len) == 0, v->name);
        if (reply_len > 0u) {
            protocol_frame_t answer;
            rack_reply_t parsed;
            TEST_ASSERT(protocol_parse_frame(reply, reply_len, &answer) == PROTOCOL_OK && rack_reply_parse(&answer, &parsed) && parsed.seq == d.reply_seq &&
                        parsed.ack == (d.acknowledge != 0u), "the reply parses back and agrees with the decision");
        }
    }
    for (int i = 0; i < RACK_VEC_COUNT(RACK_VEC_VERIFY); i++) {
        const rack_vec_verify_t *v = &RACK_VEC_VERIFY[i];
        TEST_ASSERT((int)slot_verify(v->occupied != 0, (uint16_t)v->commanded_c, v->measured_c, v->ambient_c, v->elapsed_ms) == v->verdict, v->name);
    }

    // The serial line: every stream of bytes goes through the real deframer, byte by byte with the time of its burst, and the frames that
    // come out and the counters are the ones the model of URTC-COMMON wrote down.
    int next_frame = 0;
    for (int s = 0; s < RACK_VEC_COUNT(RACK_VEC_STREAMS); s++) {
        const rack_vec_stream_t *st = &RACK_VEC_STREAMS[s];
        rack_deframer_t rx;
        rack_deframer_init(&rx, st->timeout_ms);
        int delivered = 0;
        int order_ok = 1;
        for (int e = 0; e < RACK_VEC_COUNT(RACK_VEC_STREAM_EVENTS); e++) {
            const rack_vec_stream_event_t *ev = &RACK_VEC_STREAM_EVENTS[e];
            if (ev->stream != s) {
                continue;
            }
            for (int i = 0; i < ev->len; i++) {
                uint8_t out[PROTOCOL_MAX_FRAME_SIZE];
                uint8_t len = rack_deframer_push(&rx, ev->data[i], ev->time_ms, out);
                while (len != 0u) {
                    const rack_vec_stream_frame_t *want = NULL;
                    int seen = 0;
                    for (int f = 0; f < RACK_VEC_COUNT(RACK_VEC_STREAM_FRAMES); f++) {
                        if (RACK_VEC_STREAM_FRAMES[f].stream == s) {
                            if (seen == delivered) {
                                want = &RACK_VEC_STREAM_FRAMES[f];
                                break;
                            }
                            seen++;
                        }
                    }
                    if (want == NULL || want->len != len || memcmp(want->frame, out, len) != 0) {
                        order_ok = 0;
                    }
                    delivered++;
                    len = rack_deframer_next(&rx, out);
                }
            }
        }
        int expected = 0;
        for (int f = 0; f < RACK_VEC_COUNT(RACK_VEC_STREAM_FRAMES); f++) {
            expected += RACK_VEC_STREAM_FRAMES[f].stream == s ? 1 : 0;
        }
        TEST_ASSERT(order_ok && delivered == expected, st->name);
        TEST_ASSERT(rx.stats.frames_ok == st->frames_ok && rx.stats.crc_errors == st->crc_errors && rx.stats.bad_headers == st->bad_headers &&
                    rx.stats.resyncs == st->resyncs && rx.stats.timeouts == st->timeouts && rx.stats.dropped_bytes == st->dropped_bytes, st->name);
        next_frame += expected;
    }
    TEST_ASSERT(next_frame == RACK_VEC_COUNT(RACK_VEC_STREAM_FRAMES), "every frame of the stream vectors belongs to a stream");

    // ---- free configuration of a head (jumpers 0x1F): the reference model and this code must agree -----------------------------------
    {
        const free_tool_config_t d = free_tool_config_default();
        TEST_ASSERT(d.query_timeout_ms == RACK_VEC_FREE_DEFAULTS.query_timeout_ms && d.max_attempts == RACK_VEC_FREE_DEFAULTS.max_attempts &&
                    d.retry_backoff_ms == RACK_VEC_FREE_DEFAULTS.retry_backoff_ms && d.refresh_period_ms == RACK_VEC_FREE_DEFAULTS.refresh_period_ms,
                    "the default query timing is the one of the shared contract");
    }
    for (int i = 0; i < RACK_VEC_COUNT(RACK_VEC_FREE_JUMPERS); i++) {
        const rack_vec_free_jumper_t *v = &RACK_VEC_FREE_JUMPERS[i];
        uint8_t tool = 0xFF;
        const free_tool_kind_t kind = free_tool_classify_jumpers((uint8_t)v->raw, &tool);
        TEST_ASSERT((int)kind == v->kind && (kind != FREE_TOOL_KIND_DIRECT || (int)tool == v->tool), "jumper reading classified like the shared vectors");
    }
    for (int i = 0; i < RACK_VEC_COUNT(RACK_VEC_FREE_REPLIES); i++) {
        const rack_vec_free_reply_t *v = &RACK_VEC_FREE_REPLIES[i];
        uint8_t tool = 0xFF;
        const bool ok = free_tool_parse_reply(v->payload, v->len, &tool);
        TEST_ASSERT(ok == (v->tool >= 0) && (!ok || (int)tool == v->tool), v->name);
    }
    for (int s = 0; s < RACK_VEC_COUNT(RACK_VEC_FREE_SCENARIOS); s++) {
        const rack_vec_free_scenario_t *sc = &RACK_VEC_FREE_SCENARIOS[s];
        free_tool_config_t cfg;
        cfg.query_timeout_ms = sc->config.query_timeout_ms;
        cfg.max_attempts = sc->config.max_attempts;
        cfg.retry_backoff_ms = sc->config.retry_backoff_ms;
        cfg.refresh_period_ms = sc->config.refresh_period_ms;
        free_tool_t ft;
        free_tool_reset(&ft);
        int steps_ok = 1;
        int steps = 0;
        for (int i = 0; i < RACK_VEC_COUNT(RACK_VEC_FREE_STEPS); i++) {
            const rack_vec_free_step_t *st = &RACK_VEC_FREE_STEPS[i];
            if (st->scenario != s) {
                continue;
            }
            free_tool_event_t event = FREE_TOOL_EVT_NONE;
            const bool query = free_tool_step(&ft, &cfg, st->time_ms, st->has_reply ? st->reply : NULL, st->len, &event);
            const int tool = ft.resolved ? (int)ft.tool : -1;
            if ((query ? 1 : 0) != st->query || (int)event != st->event || tool != st->tool) {
                steps_ok = 0;
            }
            steps++;
        }
        TEST_ASSERT(steps_ok && steps > 0, sc->name);
    }
}
