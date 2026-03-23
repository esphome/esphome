#pragma once
#include <cstdint>

namespace esphome {
namespace st25r {

/*
 * Crypto1 stream cipher implementation.
 *
 * The Crypto1 algorithm is documented in:
 *   Courtois, Nohl et al. "Algebraic Attacks on the Crypto-1 Stream Cipher
 *   in MiFare Classic and Oyster Cards" (2008)
 *
 * LFSR polynomial constants and filter function are mathematical facts
 * derived from the published algorithm specification.
 *
 * Protocol flow adapted from mf1.c — MIT licence, suut/rfal-mifare-classic
 *   https://github.com/suut/rfal-mifare-classic/blob/master/mf1/mf1.c
 */

struct Crypto1State {
  uint32_t odd;   // odd-indexed LFSR bits (1,3,5,...,47) packed into bits 0..23
  uint32_t even;  // even-indexed LFSR bits (0,2,4,...,46) packed into bits 0..23
};

// Initialise state from 48-bit key (MSB of key → first LFSR bit loaded)
void crypto1_init(struct Crypto1State *s, uint64_t key);

// Output filter function — operates on the odd half of the state
int crypto1_filter(uint32_t x);

// Clock one bit; in=plaintext input bit; is_encrypted=1 during encrypted phase
uint8_t crypto1_bit(struct Crypto1State *s, uint8_t in, int is_encrypted);

// Clock eight bits LSB-first
uint8_t crypto1_byte(struct Crypto1State *s, uint8_t in, int is_encrypted);

// Clock 32 bits using ISO 14443A big-endian bit ordering (used for nonce words)
uint32_t crypto1_word(struct Crypto1State *s, uint32_t in, int is_encrypted);

// Advance the tag PRNG n steps (used to predict tag nonce responses)
uint32_t prng_successor(uint32_t x, uint32_t n);

}  // namespace st25r
}  // namespace esphome
