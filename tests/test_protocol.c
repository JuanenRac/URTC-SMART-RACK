// =============================================================================
// URTC-SMART-RACK Firmware - tests for protocol.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
#include "test_runner.h"
#include "../src/protocol.h"
#include <string.h>

void run_protocol_tests(int *failures)
{
    // --- Real round trip: encode then parse gives back the exact same frame ---
    {
        uint8_t payload[3] = {5, 0xC8, 0x00}; // tool_id=5, target_temp_c=200 (little-endian)
        uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
        uint8_t written = protocol_encode_frame(7, 0x01, payload, sizeof(payload), buf);
        TEST_ASSERT(written == 5 + 3 + 1, "encode writes header + payload + CRC bytes");
        TEST_ASSERT(buf[0] == PROTOCOL_SOF, "encoded frame starts with the real SOF byte");

        protocol_frame_t frame;
        protocol_status_t status = protocol_parse_frame(buf, written, &frame);
        TEST_ASSERT(status == PROTOCOL_OK, "a real, valid encoded frame parses as PROTOCOL_OK");
        TEST_ASSERT(frame.version == PROTOCOL_VERSION, "parsed version matches");
        TEST_ASSERT(frame.seq == 7, "parsed seq matches");
        TEST_ASSERT(frame.cmd == 0x01, "parsed cmd matches");
        TEST_ASSERT(frame.len == 3, "parsed len matches");
        TEST_ASSERT(memcmp(frame.payload, payload, 3) == 0, "parsed payload bytes match exactly");
    }

    // --- Real zero-payload frame round trip ---
    {
        uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
        uint8_t written = protocol_encode_frame(1, 0x04, NULL, 0, buf);
        TEST_ASSERT(written == 6, "a zero-payload frame is exactly 6 bytes (no payload, still CRC'd)");

        protocol_frame_t frame;
        protocol_status_t status = protocol_parse_frame(buf, written, &frame);
        TEST_ASSERT(status == PROTOCOL_OK, "a real zero-payload frame parses as PROTOCOL_OK");
        TEST_ASSERT(frame.len == 0, "parsed len is 0");
    }

    // --- Real corrupted CRC is rejected, not silently accepted ---
    {
        uint8_t payload[1] = {0x42};
        uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
        uint8_t written = protocol_encode_frame(1, 0x01, payload, 1, buf);
        buf[written - 1] ^= 0xFFu; // flip every bit of the real CRC byte

        protocol_frame_t frame;
        protocol_status_t status = protocol_parse_frame(buf, written, &frame);
        TEST_ASSERT(status == PROTOCOL_ERR_CRC_MISMATCH, "a frame with a corrupted CRC byte is rejected as PROTOCOL_ERR_CRC_MISMATCH");
    }

    // --- Real corrupted payload (CRC now stale) is rejected too ---
    {
        uint8_t payload[1] = {0x42};
        uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
        uint8_t written = protocol_encode_frame(1, 0x01, payload, 1, buf);
        buf[5] ^= 0x01u; // flip a bit in the payload itself, CRC byte left as-is (now stale)

        protocol_frame_t frame;
        protocol_status_t status = protocol_parse_frame(buf, written, &frame);
        TEST_ASSERT(status == PROTOCOL_ERR_CRC_MISMATCH, "a frame with corrupted payload (stale CRC) is rejected as PROTOCOL_ERR_CRC_MISMATCH");
    }

    // --- Real bad SOF ---
    {
        uint8_t buf[6] = {0x00, PROTOCOL_VERSION, 0, 0, 0, 0};
        protocol_frame_t frame;
        TEST_ASSERT(protocol_parse_frame(buf, 6, &frame) == PROTOCOL_ERR_BAD_SOF, "a frame with the wrong SOF byte is rejected as PROTOCOL_ERR_BAD_SOF");
    }

    // --- Real unsupported version ---
    {
        uint8_t payload[1] = {0};
        uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
        uint8_t written = protocol_encode_frame(1, 0x01, payload, 1, buf);
        buf[1] = (uint8_t)(PROTOCOL_VERSION + 1); // bump the version byte after a real, otherwise-valid encode
        // The CRC now covers a different VERSION byte than what was
        // signed, so a real receiver would reject this as a CRC mismatch
        // first - re-encode the CRC to isolate the version check itself.
        buf[written - 1] = protocol_crc8(buf, written - 1);

        protocol_frame_t frame;
        TEST_ASSERT(protocol_parse_frame(buf, written, &frame) == PROTOCOL_ERR_UNSUPPORTED_VERSION, "a real frame with a future protocol version is rejected as PROTOCOL_ERR_UNSUPPORTED_VERSION");
    }

    // --- Real length declared beyond PROTOCOL_MAX_PAYLOAD ---
    {
        uint8_t buf[7] = {PROTOCOL_SOF, PROTOCOL_VERSION, 0, 0, PROTOCOL_MAX_PAYLOAD + 1, 0, 0};
        protocol_frame_t frame;
        TEST_ASSERT(protocol_parse_frame(buf, 7, &frame) == PROTOCOL_ERR_LENGTH_OUT_OF_RANGE, "a declared length beyond PROTOCOL_MAX_PAYLOAD is rejected as PROTOCOL_ERR_LENGTH_OUT_OF_RANGE");
    }

    // --- Real incomplete buffer (fewer bytes than the frame declares) ---
    {
        uint8_t payload[3] = {1, 2, 3};
        uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
        uint8_t written = protocol_encode_frame(1, 0x01, payload, 3, buf);
        protocol_frame_t frame;
        TEST_ASSERT(protocol_parse_frame(buf, (uint8_t)(written - 1), &frame) == PROTOCOL_ERR_INCOMPLETE, "a buffer one byte short of a real declared frame is PROTOCOL_ERR_INCOMPLETE, not treated as corrupt");
        TEST_ASSERT(protocol_parse_frame(buf, 3, &frame) == PROTOCOL_ERR_INCOMPLETE, "a buffer shorter than even the fixed header is PROTOCOL_ERR_INCOMPLETE");
    }

    // --- encode_frame rejects an over-long payload rather than truncating it ---
    {
        uint8_t oversized_payload[PROTOCOL_MAX_PAYLOAD + 1];
        memset(oversized_payload, 0xAA, sizeof(oversized_payload));
        uint8_t buf[PROTOCOL_MAX_FRAME_SIZE + 1];
        TEST_ASSERT(protocol_encode_frame(1, 0x01, oversized_payload, sizeof(oversized_payload), buf) == 0, "encode_frame refuses a payload longer than PROTOCOL_MAX_PAYLOAD rather than silently truncating it");
    }
}
