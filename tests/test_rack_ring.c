// =============================================================================
// URTC-SMART-RACK Firmware - tests for rack_ring.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include "test_runner.h"
#include "../src/rack_ring.h"

void run_rack_ring_tests(int *failures)
{
    uint8_t storage[8];
    uint8_t byte = 0;
    rack_ring_t ring;

    TEST_ASSERT(!rack_ring_init(NULL, storage, 8), "init refuses a NULL ring");
    TEST_ASSERT(!rack_ring_init(&ring, NULL, 8), "init refuses NULL storage");
    TEST_ASSERT(!rack_ring_init(&ring, storage, 0), "init refuses size 0");
    TEST_ASSERT(!rack_ring_init(&ring, storage, 1), "init refuses size 1");
    TEST_ASSERT(!rack_ring_init(&ring, storage, 6), "init refuses a size that is not a power of two");
    TEST_ASSERT(!rack_ring_init(&ring, storage, 65535), "init refuses a size past 32768");
    TEST_ASSERT(!rack_ring_put(&ring, 1), "a ring that failed to initialise accepts nothing");
    TEST_ASSERT(!rack_ring_get(&ring, &byte), "a ring that failed to initialise returns nothing");
    TEST_ASSERT(rack_ring_count(&ring) == 0, "a ring that failed to initialise is empty");
    TEST_ASSERT(!rack_ring_put(NULL, 1), "put on NULL is refused");
    TEST_ASSERT(!rack_ring_get(NULL, &byte), "get on NULL is refused");
    TEST_ASSERT(rack_ring_count(NULL) == 0, "count on NULL is zero");
    TEST_ASSERT(rack_ring_take_dropped(NULL) == 0, "take_dropped on NULL is zero");

    TEST_ASSERT(rack_ring_init(&ring, storage, 8), "a power-of-two size initialises");
    TEST_ASSERT(!rack_ring_get(&ring, &byte), "an empty ring returns nothing");
    TEST_ASSERT(!rack_ring_get(&ring, NULL), "get refuses a NULL destination");

    for (uint8_t i = 0; i < 7; i++) {
        TEST_ASSERT(rack_ring_put(&ring, (uint8_t)(0x10 + i)), "seven bytes fit in a ring of eight");
    }
    TEST_ASSERT(rack_ring_count(&ring) == 7, "the count is seven");
    TEST_ASSERT(!rack_ring_put(&ring, 0xEE), "the eighth byte is dropped: one slot stays free");
    TEST_ASSERT(!rack_ring_put(&ring, 0xEF), "so is the ninth");
    TEST_ASSERT(rack_ring_take_dropped(&ring) == 2, "two bytes were dropped");
    TEST_ASSERT(rack_ring_take_dropped(&ring) == 0, "the drop count is reported once");
    for (uint8_t i = 0; i < 7; i++) {
        TEST_ASSERT(rack_ring_get(&ring, &byte) && byte == (uint8_t)(0x10 + i), "bytes come out in the order they went in");
    }
    TEST_ASSERT(!rack_ring_get(&ring, &byte), "the ring is empty again");
    TEST_ASSERT(rack_ring_count(&ring) == 0, "the count is zero again");

    // Many times around the ring, with the producer and the consumer interleaved at different rates.
    uint8_t next_in = 0;
    uint8_t next_out = 0;
    bool in_order = true;
    for (int round = 0; round < 1000; round++) {
        const int burst = 1 + (round % 5);
        for (int i = 0; i < burst; i++) {
            if (rack_ring_put(&ring, next_in)) {
                next_in++;
            }
        }
        const int take = 1 + (round % 3);
        for (int i = 0; i < take; i++) {
            if (rack_ring_get(&ring, &byte)) {
                if (byte != next_out) {
                    in_order = false;
                }
                next_out++;
            }
        }
    }
    while (rack_ring_get(&ring, &byte)) {
        if (byte != next_out) {
            in_order = false;
        }
        next_out++;
    }
    TEST_ASSERT(in_order, "thousands of interleaved bytes keep their order across the wrap");
    TEST_ASSERT(next_in == next_out, "nothing that was accepted is lost");
    TEST_ASSERT(rack_ring_count(&ring) == 0, "the ring ends empty");

    // The drop counter wraps without losing the difference between two reads.
    rack_ring_t small;
    uint8_t two[2];
    TEST_ASSERT(rack_ring_init(&small, two, 2), "the smallest ring initialises");
    TEST_ASSERT(rack_ring_put(&small, 1), "a ring of two holds one byte");
    TEST_ASSERT(!rack_ring_put(&small, 2), "and drops the second");
    small.dropped = 0xFFFE;
    small.dropped_seen = 0xFFFE;
    TEST_ASSERT(!rack_ring_put(&small, 3) && !rack_ring_put(&small, 4) && !rack_ring_put(&small, 5), "three more drops across the 16-bit wrap");
    TEST_ASSERT(rack_ring_take_dropped(&small) == 3, "the count survives the wrap of the running counter");
}
