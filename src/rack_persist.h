// =============================================================================
// URTC-SMART-RACK Firmware - Lifecycle and inventory kept across power cuts: rack_persist.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// What the rack must remember when the power goes away (the cycles and hours of use of the tools and
// the slot inventory) is kept in two copies, A and B, of one fixed-size record in a non-volatile memory
// (an FRAM or an EEPROM). The record has a magic, a structure version, a sequence number and a CRC-8
// (the polynomial 0x07 that the link and the SavedState_t of the tool head already use). A save writes
// the copy that does NOT hold the newest valid state, and publishes it by writing the magic LAST, so a
// cut at any byte of a save leaves the older copy intact and the half-written one unreadable: a load
// returns the newest copy that is complete, never a mixture.
//
// The memory itself is behind `rack_store_t` (two callbacks), so the pure logic is tested on the host
// with a store that loses power after any number of bytes, and the day the board exists only the two
// callbacks (I2C/SPI to the FRAM) are written.
#ifndef RACK_PERSIST_H
#define RACK_PERSIST_H

#include <stdbool.h>
#include <stdint.h>

#include "lifecycle.h"
#include "rack_inventory.h"

#define RACK_PERSIST_MAGIC        0xA7u
#define RACK_PERSIST_VERSION      1u
#define RACK_PERSIST_RECORD_SIZE  55u    // magic, version, sequence(4), cycles(4), seconds(4), 8 slots x 5 bytes, crc8
#define RACK_PERSIST_COPY_STRIDE  64u    // copy A at offset 0, copy B at offset 64

// A non-volatile memory. Both return false on a failed access. `ctx` is passed back unchanged.
typedef struct {
    bool (*read)(void *ctx, uint32_t offset, uint8_t *dst, uint8_t len);
    bool (*write)(void *ctx, uint32_t offset, const uint8_t *src, uint8_t len);
    void *ctx;
} rack_store_t;

typedef struct {
    lifecycle_t lifecycle;
    rack_inventory_t inventory;
} rack_state_t;

typedef enum {
    RACK_PERSIST_OK = 0,
    RACK_PERSIST_EMPTY,       // no copy holds a valid record (a new or erased memory): the caller starts from its defaults
    RACK_PERSIST_IO_ERROR,    // the memory could not be read or written
    RACK_PERSIST_BAD_ARGUMENT
} rack_persist_status_t;

// Loads the newest valid copy. On RACK_PERSIST_OK, `state` is filled and `*sequence` (may be NULL) is its sequence number.
// On any other status `state` is left untouched.
rack_persist_status_t rack_persist_load(const rack_store_t *store, rack_state_t *state, uint32_t *sequence);

// Saves `state` as the new newest copy (sequence = previous newest + 1, or 1 on an empty memory).
rack_persist_status_t rack_persist_save(const rack_store_t *store, const rack_state_t *state);

#endif // RACK_PERSIST_H
