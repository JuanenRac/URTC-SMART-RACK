// =============================================================================
// URTC-SMART-RACK Firmware - tests for rack_persist.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include "test_runner.h"
#include "../src/rack_persist.h"
#include "../src/urtc_crc8.h"
#include <string.h>

// A memory that can lose power: after `budget` bytes have been written, every write stops where it is.
typedef struct {
    uint8_t bytes[2 * RACK_PERSIST_COPY_STRIDE];
    long budget;       // bytes that can still be written; < 0 means unlimited
    bool fail_reads;
    bool fail_writes;
} fake_memory_t;

static bool mem_read(void *ctx, uint32_t offset, uint8_t *dst, uint8_t len)
{
    fake_memory_t *m = (fake_memory_t *)ctx;
    if (m->fail_reads || offset + len > sizeof m->bytes) {
        return false;
    }
    memcpy(dst, &m->bytes[offset], len);
    return true;
}

static bool mem_write(void *ctx, uint32_t offset, const uint8_t *src, uint8_t len)
{
    fake_memory_t *m = (fake_memory_t *)ctx;
    if (m->fail_writes || offset + len > sizeof m->bytes) {
        return false;
    }
    for (uint8_t i = 0; i < len; i++) {
        if (m->budget == 0) {
            return true;       // power is gone: the rest of the write never reaches the memory, and nobody is left to notice
        }
        m->bytes[offset + i] = src[i];
        if (m->budget > 0) {
            m->budget--;
        }
    }
    return true;
}

// Recomputes the CRC of copy A after a test edited a byte of it on purpose.
static void refresh_crc(fake_memory_t *m)
{
    m->bytes[RACK_PERSIST_RECORD_SIZE - 1u] = urtc_crc8(m->bytes, (uint8_t)(RACK_PERSIST_RECORD_SIZE - 1u));
}

static rack_store_t store_of(fake_memory_t *m)
{
    rack_store_t s = { mem_read, mem_write, m };
    return s;
}

static rack_state_t make_state(uint32_t cycles)
{
    rack_state_t st;
    memset(&st, 0, sizeof st);
    st.lifecycle.total_cycles = cycles;
    st.lifecycle.total_seconds = cycles * 60u;
    for (uint8_t i = 0; i < RACK_INVENTORY_MAX_SLOTS; i++) {
        st.inventory.slots[i].occupied = (i % 2u) == 0u;
        st.inventory.slots[i].tool_id = (uint8_t)(i + 1u);
        st.inventory.slots[i].serial = (uint8_t)(cycles + i);
        st.inventory.slots[i].target_temp_c = (uint16_t)(i == 3u ? 300u : 0u);     // above 255 on purpose
    }
    return st;
}

static bool same(const rack_state_t *a, const rack_state_t *b)
{
    return memcmp(a, b, sizeof *a) == 0;
}

void run_rack_persist_tests(int *failures)
{
    fake_memory_t mem = { {0}, -1, false, false };
    rack_state_t loaded;
    uint32_t sequence = 0;
    memset(mem.bytes, 0xFF, sizeof mem.bytes);        // an erased FRAM/EEPROM reads as 0xFF
    mem.budget = -1;
    rack_store_t store = store_of(&mem);

    TEST_ASSERT(rack_persist_load(NULL, &loaded, NULL) == RACK_PERSIST_BAD_ARGUMENT, "a NULL store is refused");
    TEST_ASSERT(rack_persist_load(&store, NULL, NULL) == RACK_PERSIST_BAD_ARGUMENT, "a NULL state is refused");
    TEST_ASSERT(rack_persist_save(&store, NULL) == RACK_PERSIST_BAD_ARGUMENT, "saving a NULL state is refused");
    rack_store_t half = { mem_read, NULL, &mem };
    rack_state_t first = make_state(1);
    TEST_ASSERT(rack_persist_save(&half, &first) == RACK_PERSIST_BAD_ARGUMENT, "a store with no write callback is refused");

    TEST_ASSERT(rack_persist_load(&store, &loaded, &sequence) == RACK_PERSIST_EMPTY, "an erased memory has no state");
    memset(mem.bytes, 0x00, sizeof mem.bytes);
    TEST_ASSERT(rack_persist_load(&store, &loaded, &sequence) == RACK_PERSIST_EMPTY, "a memory of zeros has no state");
    memset(mem.bytes, 0xFF, sizeof mem.bytes);

    TEST_ASSERT(rack_persist_save(&store, &first) == RACK_PERSIST_OK, "the first save works");
    TEST_ASSERT(rack_persist_load(&store, &loaded, &sequence) == RACK_PERSIST_OK, "and loads");
    TEST_ASSERT(same(&loaded, &first) && sequence == 1u, "with the same state and sequence 1 (a target of 300 degrees included)");
    TEST_ASSERT(mem.bytes[0] == RACK_PERSIST_MAGIC, "the first save goes to copy A");

    rack_state_t second = make_state(2);
    TEST_ASSERT(rack_persist_save(&store, &second) == RACK_PERSIST_OK, "the second save works");
    TEST_ASSERT(mem.bytes[RACK_PERSIST_COPY_STRIDE] == RACK_PERSIST_MAGIC, "the second save goes to copy B");
    TEST_ASSERT(rack_persist_load(&store, &loaded, &sequence) == RACK_PERSIST_OK && same(&loaded, &second) && sequence == 2u, "the newest copy is returned");
    rack_state_t third = make_state(3);
    TEST_ASSERT(rack_persist_save(&store, &third) == RACK_PERSIST_OK, "the third save works");
    TEST_ASSERT(rack_persist_load(&store, &loaded, &sequence) == RACK_PERSIST_OK && same(&loaded, &third) && sequence == 3u, "it alternates and the newest wins");

    // A damaged copy is not used: the other one is.
    mem.bytes[20] ^= 0x01;       // inside copy A (the older one, sequence 3 is in A: copy A holds the third save)
    rack_persist_status_t status = rack_persist_load(&store, &loaded, &sequence);
    TEST_ASSERT(status == RACK_PERSIST_OK && same(&loaded, &second) && sequence == 2u, "a flipped bit in the newest copy falls back to the older one");
    mem.bytes[20] ^= 0x01;
    mem.bytes[RACK_PERSIST_COPY_STRIDE + 20] ^= 0x80;
    status = rack_persist_load(&store, &loaded, &sequence);
    TEST_ASSERT(status == RACK_PERSIST_OK && same(&loaded, &third), "a flipped bit in the older copy changes nothing");
    mem.bytes[20] ^= 0x01;
    TEST_ASSERT(rack_persist_load(&store, &loaded, &sequence) == RACK_PERSIST_EMPTY, "both copies damaged means no state, never a guess");

    // A structure version this firmware does not know is not read.
    memset(mem.bytes, 0xFF, sizeof mem.bytes);
    TEST_ASSERT(rack_persist_save(&store, &first) == RACK_PERSIST_OK, "a save on a cleared memory works");
    mem.bytes[1] = (uint8_t)(RACK_PERSIST_VERSION + 1u);
    TEST_ASSERT(rack_persist_load(&store, &loaded, &sequence) == RACK_PERSIST_EMPTY, "another structure version is not read");

    // A flag that is neither 0 nor 1, with a CRC that is valid for it, is still refused.
    memset(mem.bytes, 0xFF, sizeof mem.bytes);
    TEST_ASSERT(rack_persist_save(&store, &first) == RACK_PERSIST_OK, "a save for the flag test works");
    mem.bytes[14] = 2u;
    refresh_crc(&mem);
    TEST_ASSERT(rack_persist_load(&store, &loaded, &sequence) == RACK_PERSIST_EMPTY, "an impossible occupied flag is refused even with a good CRC");

    // Failing memory.
    memset(mem.bytes, 0xFF, sizeof mem.bytes);
    mem.fail_reads = true;
    TEST_ASSERT(rack_persist_load(&store, &loaded, NULL) == RACK_PERSIST_IO_ERROR, "a failing read is reported");
    TEST_ASSERT(rack_persist_save(&store, &first) == RACK_PERSIST_IO_ERROR, "a save that cannot read the copies does not write");
    mem.fail_reads = false;
    mem.fail_writes = true;
    TEST_ASSERT(rack_persist_save(&store, &first) == RACK_PERSIST_IO_ERROR, "a failing write is reported");
    mem.fail_writes = false;

    // The sequence number wraps: 0xFFFFFFFF is followed by 0, and 0 is the newer one.
    memset(mem.bytes, 0xFF, sizeof mem.bytes);
    TEST_ASSERT(rack_persist_save(&store, &first) == RACK_PERSIST_OK, "a save for the wrap test works");
    mem.bytes[2] = 0xFE; mem.bytes[3] = 0xFF; mem.bytes[4] = 0xFF; mem.bytes[5] = 0xFF;
    refresh_crc(&mem);
    TEST_ASSERT(rack_persist_save(&store, &second) == RACK_PERSIST_OK, "the save after sequence 0xFFFFFFFE works");
    TEST_ASSERT(rack_persist_save(&store, &third) == RACK_PERSIST_OK, "the save after 0xFFFFFFFF works");
    TEST_ASSERT(rack_persist_load(&store, &loaded, &sequence) == RACK_PERSIST_OK && sequence == 0u && same(&loaded, &third), "sequence 0 after 0xFFFFFFFF is the newest");

    // Power lost at every byte of a save: the state is always the old one or the new one, never anything else.
    bool never_a_mixture = true;
    bool both_outcomes_seen_old = false;
    bool both_outcomes_seen_new = false;
    for (int copies_before = 0; copies_before < 2; copies_before++) {
        for (long cut = 0; cut <= (long)RACK_PERSIST_RECORD_SIZE + 1; cut++) {
            memset(mem.bytes, 0xFF, sizeof mem.bytes);
            mem.budget = -1;
            rack_state_t old_state = make_state(10);
            rack_state_t new_state = make_state(11);
            if (rack_persist_save(&store, &old_state) != RACK_PERSIST_OK) {
                never_a_mixture = false;
            }
            if (copies_before == 1 && rack_persist_save(&store, &old_state) != RACK_PERSIST_OK) {
                never_a_mixture = false;
            }
            mem.budget = cut;
            (void)rack_persist_save(&store, &new_state);
            mem.budget = -1;
            if (rack_persist_load(&store, &loaded, &sequence) != RACK_PERSIST_OK) {
                never_a_mixture = false;
            } else if (same(&loaded, &old_state)) {
                both_outcomes_seen_old = true;
            } else if (same(&loaded, &new_state)) {
                both_outcomes_seen_new = true;
            } else {
                never_a_mixture = false;
            }
        }
    }
    TEST_ASSERT(never_a_mixture, "power lost at any byte of a save leaves the old state or the new one, never a mixture");
    TEST_ASSERT(both_outcomes_seen_old && both_outcomes_seen_new, "the cut sweep really covers both a lost save and a completed one");
}
