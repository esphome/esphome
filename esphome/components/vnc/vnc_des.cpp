#include "vnc_des.h"

#include <cstddef>

#include "esphome/core/hal.h"

namespace esphome::vnc {

// Tables from FIPS 46-3. Entries are 1-based bit positions, bit 1 being the most significant.
static const uint8_t INITIAL_PERMUTATION[64] PROGMEM = {
    58, 50, 42, 34, 26, 18, 10, 2, 60, 52, 44, 36, 28, 20, 12, 4,  //
    62, 54, 46, 38, 30, 22, 14, 6, 64, 56, 48, 40, 32, 24, 16, 8,  //
    57, 49, 41, 33, 25, 17, 9,  1, 59, 51, 43, 35, 27, 19, 11, 3,  //
    61, 53, 45, 37, 29, 21, 13, 5, 63, 55, 47, 39, 31, 23, 15, 7,  //
};

static const uint8_t FINAL_PERMUTATION[64] PROGMEM = {
    40, 8, 48, 16, 56, 24, 64, 32, 39, 7, 47, 15, 55, 23, 63, 31,  //
    38, 6, 46, 14, 54, 22, 62, 30, 37, 5, 45, 13, 53, 21, 61, 29,  //
    36, 4, 44, 12, 52, 20, 60, 28, 35, 3, 43, 11, 51, 19, 59, 27,  //
    34, 2, 42, 10, 50, 18, 58, 26, 33, 1, 41, 9,  49, 17, 57, 25,  //
};

static const uint8_t EXPANSION[48] PROGMEM = {
    32, 1,  2,  3,  4,  5,  4,  5,  6,  7,  8,  9,  8,  9,  10, 11,  //
    12, 13, 12, 13, 14, 15, 16, 17, 16, 17, 18, 19, 20, 21, 20, 21,  //
    22, 23, 24, 25, 24, 25, 26, 27, 28, 29, 28, 29, 30, 31, 32, 1,   //
};

static const uint8_t ROUND_PERMUTATION[32] PROGMEM = {
    16, 7, 20, 21, 29, 12, 28, 17, 1,  15, 23, 26, 5,  18, 31, 10,  //
    2,  8, 24, 14, 32, 27, 3,  9,  19, 13, 30, 6,  22, 11, 4,  25,  //
};

static const uint8_t PERMUTED_CHOICE_1[56] PROGMEM = {
    57, 49, 41, 33, 25, 17, 9,  1,  58, 50, 42, 34, 26, 18,  //
    10, 2,  59, 51, 43, 35, 27, 19, 11, 3,  60, 52, 44, 36,  //
    63, 55, 47, 39, 31, 23, 15, 7,  62, 54, 46, 38, 30, 22,  //
    14, 6,  61, 53, 45, 37, 29, 21, 13, 5,  28, 20, 12, 4,   //
};

static const uint8_t PERMUTED_CHOICE_2[48] PROGMEM = {
    14, 17, 11, 24, 1,  5,  3,  28, 15, 6,  21, 10, 23, 19, 12, 4,   //
    26, 8,  16, 7,  27, 20, 13, 2,  41, 52, 31, 37, 47, 55, 30, 40,  //
    51, 45, 33, 48, 44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32,  //
};

// S1 to S8, one row of 16 entries per line, two entries per byte (high nibble first).
static const uint8_t S_BOXES[256] PROGMEM = {
    0xE4, 0xD1, 0x2F, 0xB8, 0x3A, 0x6C, 0x59, 0x07,  //
    0x0F, 0x74, 0xE2, 0xD1, 0xA6, 0xCB, 0x95, 0x38,  //
    0x41, 0xE8, 0xD6, 0x2B, 0xFC, 0x97, 0x3A, 0x50,  //
    0xFC, 0x82, 0x49, 0x17, 0x5B, 0x3E, 0xA0, 0x6D,  //
    0xF1, 0x8E, 0x6B, 0x34, 0x97, 0x2D, 0xC0, 0x5A,  //
    0x3D, 0x47, 0xF2, 0x8E, 0xC0, 0x1A, 0x69, 0xB5,  //
    0x0E, 0x7B, 0xA4, 0xD1, 0x58, 0xC6, 0x93, 0x2F,  //
    0xD8, 0xA1, 0x3F, 0x42, 0xB6, 0x7C, 0x05, 0xE9,  //
    0xA0, 0x9E, 0x63, 0xF5, 0x1D, 0xC7, 0xB4, 0x28,  //
    0xD7, 0x09, 0x34, 0x6A, 0x28, 0x5E, 0xCB, 0xF1,  //
    0xD6, 0x49, 0x8F, 0x30, 0xB1, 0x2C, 0x5A, 0xE7,  //
    0x1A, 0xD0, 0x69, 0x87, 0x4F, 0xE3, 0xB5, 0x2C,  //
    0x7D, 0xE3, 0x06, 0x9A, 0x12, 0x85, 0xBC, 0x4F,  //
    0xD8, 0xB5, 0x6F, 0x03, 0x47, 0x2C, 0x1A, 0xE9,  //
    0xA6, 0x90, 0xCB, 0x7D, 0xF1, 0x3E, 0x52, 0x84,  //
    0x3F, 0x06, 0xA1, 0xD8, 0x94, 0x5B, 0xC7, 0x2E,  //
    0x2C, 0x41, 0x7A, 0xB6, 0x85, 0x3F, 0xD0, 0xE9,  //
    0xEB, 0x2C, 0x47, 0xD1, 0x50, 0xFA, 0x39, 0x86,  //
    0x42, 0x1B, 0xAD, 0x78, 0xF9, 0xC5, 0x63, 0x0E,  //
    0xB8, 0xC7, 0x1E, 0x2D, 0x6F, 0x09, 0xA4, 0x53,  //
    0xC1, 0xAF, 0x92, 0x68, 0x0D, 0x34, 0xE7, 0x5B,  //
    0xAF, 0x42, 0x7C, 0x95, 0x61, 0xDE, 0x0B, 0x38,  //
    0x9E, 0xF5, 0x28, 0xC3, 0x70, 0x4A, 0x1D, 0xB6,  //
    0x43, 0x2C, 0x95, 0xFA, 0xBE, 0x17, 0x60, 0x8D,  //
    0x4B, 0x2E, 0xF0, 0x8D, 0x3C, 0x97, 0x5A, 0x61,  //
    0xD0, 0xB7, 0x49, 0x1A, 0xE3, 0x5C, 0x2F, 0x86,  //
    0x14, 0xBD, 0xC3, 0x7E, 0xAF, 0x68, 0x05, 0x92,  //
    0x6B, 0xD8, 0x14, 0xA7, 0x95, 0x0F, 0xE2, 0x3C,  //
    0xD2, 0x84, 0x6F, 0xB1, 0xA9, 0x3E, 0x50, 0xC7,  //
    0x1F, 0xD8, 0xA3, 0x74, 0xC5, 0x6B, 0x0E, 0x92,  //
    0x7B, 0x41, 0x9C, 0xE2, 0x06, 0xAD, 0xF3, 0x58,  //
    0x21, 0xE7, 0x4A, 0x8D, 0xFC, 0x90, 0x35, 0x6B,  //
};

// Bit n set: round n rotates the key halves by two bits instead of one.
static constexpr uint16_t DOUBLE_SHIFT_ROUNDS = 0x7EFC;
static constexpr uint8_t ROUNDS = 16;
static constexpr uint32_t HALF_KEY_MASK = 0x0FFFFFFF;

// Kept out of line: inlining it at all six call sites costs about 200 bytes of flash.
static uint64_t __attribute__((noinline)) permute(uint64_t in, const uint8_t *table, uint8_t out_bits,
                                                  uint8_t in_bits) {
  uint64_t out = 0;
  for (uint8_t i = 0; i < out_bits; i++) {
    out = (out << 1) | ((in >> (in_bits - progmem_read_byte(table + i))) & 1);
  }
  return out;
}

static uint64_t load_block(const uint8_t *bytes) {
  uint64_t value = 0;
  for (uint8_t i = 0; i < 8; i++) {
    value = (value << 8) | bytes[i];
  }
  return value;
}

static void store_block(uint64_t value, uint8_t *bytes) {
  for (int8_t i = 7; i >= 0; i--) {
    bytes[i] = static_cast<uint8_t>(value);
    value >>= 8;
  }
}

static void wipe(void *data, size_t len) {
  volatile auto *p = static_cast<volatile uint8_t *>(data);
  for (size_t i = 0; i < len; i++) {
    p[i] = 0;
  }
}

static void key_schedule(const uint8_t key[8], uint64_t subkeys[ROUNDS]) {
  uint64_t halves = permute(load_block(key), PERMUTED_CHOICE_1, 56, 64);
  uint32_t c = static_cast<uint32_t>(halves >> 28);
  uint32_t d = static_cast<uint32_t>(halves) & HALF_KEY_MASK;
  for (uint8_t round = 0; round < ROUNDS; round++) {
    uint8_t shift = ((DOUBLE_SHIFT_ROUNDS >> round) & 1) ? 2 : 1;
    c = ((c << shift) | (c >> (28 - shift))) & HALF_KEY_MASK;
    d = ((d << shift) | (d >> (28 - shift))) & HALF_KEY_MASK;
    subkeys[round] = permute((static_cast<uint64_t>(c) << 28) | d, PERMUTED_CHOICE_2, 48, 56);
  }
}

static uint32_t feistel(uint32_t right, uint64_t subkey) {
  uint64_t mixed = permute(right, EXPANSION, 48, 32) ^ subkey;
  uint32_t out = 0;
  for (uint8_t box = 0; box < 8; box++) {
    uint8_t six = (mixed >> (42 - 6 * box)) & 0x3F;
    // Row is the outer two bits, column the middle four.
    uint8_t index = (six & 0x20) | ((six & 0x01) << 4) | ((six >> 1) & 0x0F);
    uint8_t packed = progmem_read_byte(&S_BOXES[box * 32 + index / 2]);
    out = (out << 4) | ((index & 1) ? (packed & 0x0F) : (packed >> 4));
  }
  return static_cast<uint32_t>(permute(out, ROUND_PERMUTATION, 32, 32));
}

static void encrypt_block(const uint64_t subkeys[ROUNDS], uint8_t block[8]) {
  uint64_t state = permute(load_block(block), INITIAL_PERMUTATION, 64, 64);
  uint32_t left = static_cast<uint32_t>(state >> 32);
  uint32_t right = static_cast<uint32_t>(state);
  for (uint8_t round = 0; round < ROUNDS; round++) {
    uint32_t next = left ^ feistel(right, subkeys[round]);
    left = right;
    right = next;
  }
  store_block(permute((static_cast<uint64_t>(right) << 32) | left, FINAL_PERMUTATION, 64, 64), block);
}

void des_encrypt_block(const uint8_t key[8], uint8_t block[8]) {
  uint64_t subkeys[ROUNDS];
  key_schedule(key, subkeys);
  encrypt_block(subkeys, block);
  wipe(subkeys, sizeof(subkeys));
}

void vnc_auth_response(const uint8_t *key_progmem, uint8_t challenge[16]) {
  uint8_t key[8];
  for (uint8_t i = 0; i < 8; i++) {
    key[i] = progmem_read_byte(key_progmem + i);
  }
  uint64_t subkeys[ROUNDS];
  key_schedule(key, subkeys);
  wipe(key, sizeof(key));
  encrypt_block(subkeys, challenge);
  encrypt_block(subkeys, challenge + 8);
  wipe(subkeys, sizeof(subkeys));
}

}  // namespace esphome::vnc
