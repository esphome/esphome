/*
 * Crypto1 stream cipher — standalone C++ implementation.
 *
 * Algorithm: Courtois, Nohl et al. (2008) — public academic specification.
 * LFSR polynomial constants (LF_POLY_ODD/EVEN) and filter function constants
 * are derived from the published algorithm and are mathematical facts.
 *
 * Protocol flow inspired by mf1.c (MIT, suut/rfal-mifare-classic):
 *   https://github.com/suut/rfal-mifare-classic/blob/master/mf1/mf1.c
 */

#include "crypto1.h"
#include <cstring>

namespace esphome {
namespace st25r {

// ── LFSR feedback polynomial masks ─────────────────────────────────────────
// The 48-bit Crypto1 LFSR uses two interleaved 24-bit words (odd/even).
// These masks select the tap positions in each half-word; the new LFSR bit is
// evenparity(LF_POLY_ODD & odd) XOR evenparity(LF_POLY_EVEN & even) XOR input.
#define LF_POLY_ODD  (0x29CE5C)
#define LF_POLY_EVEN (0x870804)

#define BIT(x, n)   (((x) >> (n)) & 1)
#define BEBIT(x, n) BIT((x), (n) ^ 24)

static inline uint8_t evenparity32(uint32_t x) {
#if defined(__GNUC__)
  return (uint8_t) __builtin_parity(x);
#else
  x ^= x >> 16;
  x ^= x >> 8;
  x ^= x >> 4;
  x ^= x >> 2;
  x ^= x >> 1;
  return x & 1;
#endif
}

// ── Output filter function ─────────────────────────────────────────────────
// Five-variable boolean function f20 operating on the odd LFSR half-word.
// Constants encode the function's truth table across the 5 input nibbles;
// the final lookup is into the 32-bit constant 0xEC57E80A.
int crypto1_filter(uint32_t const x) {
  uint32_t f;
  f  = 0xf22c0 >> (x        & 0xf) & 16;
  f |= 0x6c9c0 >> (x >>  4  & 0xf) &  8;
  f |= 0x3c8b0 >> (x >>  8  & 0xf) &  4;
  f |= 0x1e458 >> (x >> 12  & 0xf) &  2;
  f |= 0x0d938 >> (x >> 16  & 0xf) &  1;
  return BIT(0xEC57E80A, f);
}

// ── Initialise ─────────────────────────────────────────────────────────────
// Load 48-bit key into the interleaved odd/even state.
// Key bit 47 (MSB after XOR-8 reorder) → first bit loaded.
void crypto1_init(struct Crypto1State *s, uint64_t key) {
  s->odd = 0;
  s->even = 0;
  for (int i = 47; i > 0; i -= 2) {
    s->odd  = s->odd  << 1 | BIT(key, (i - 1) ^ 7);
    s->even = s->even << 1 | BIT(key,  i       ^ 7);
  }
}

// ── Clock one bit ──────────────────────────────────────────────────────────
uint8_t crypto1_bit(struct Crypto1State *s, uint8_t in, int is_encrypted) {
  uint32_t feedin, t;
  uint8_t ret = (uint8_t) crypto1_filter(s->odd);

  feedin  = ret & (uint32_t)(!!is_encrypted);
  feedin ^= (uint32_t)(!!in);
  feedin ^= LF_POLY_ODD  & s->odd;
  feedin ^= LF_POLY_EVEN & s->even;
  s->even = s->even << 1 | evenparity32(feedin);

  t = s->odd;
  s->odd = s->even;
  s->even = t;

  return ret;
}

// ── Clock one byte (LSB first) ─────────────────────────────────────────────
uint8_t crypto1_byte(struct Crypto1State *s, uint8_t in, int is_encrypted) {
  uint8_t ret = 0;
  for (int i = 0; i < 8; i++)
    ret |= (uint8_t)(crypto1_bit(s, BIT(in, i), is_encrypted) << i);
  return ret;
}

// ── Clock 32 bits (big-endian ISO 14443A bit order) ────────────────────────
uint32_t crypto1_word(struct Crypto1State *s, uint32_t in, int is_encrypted) {
  uint32_t ret = 0;
  for (int i = 0; i < 32; i++)
    ret |= (uint32_t)(crypto1_bit(s, BEBIT(in, i), is_encrypted) << (24 ^ i));
  return ret;
}

// ── Tag PRNG ───────────────────────────────────────────────────────────────
// 32-bit Galois LFSR used to generate the tag's nonce sequence.
// Taps at bits 16, 18, 19, 21 (0-indexed from LSB, big-endian byte order).
static inline uint32_t swapendian(uint32_t x) {
  x = ((x >> 8) & 0x00ff00ffu) | ((x & 0x00ff00ffu) << 8);
  x = (x >> 16) | (x << 16);
  return x;
}

uint32_t prng_successor(uint32_t x, uint32_t n) {
  x = swapendian(x);
  while (n--)
    x = x >> 1 | (x >> 16 ^ x >> 18 ^ x >> 19 ^ x >> 21) << 31;
  return swapendian(x);
}

}  // namespace st25r
}  // namespace esphome
