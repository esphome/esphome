/*
 * Copyright (C) 2015 Southern Storm Software, Pty Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "AES.h"
#include "Crypto.h"
namespace esphome {
namespace dsmr {
#if defined(DSMR_CRYPTO_AES_DEFAULT)

/**
 * \class AESCommon AES.h <AES.h>
 * \brief Abstract base class for AES block ciphers.
 *
 * This class is abstract.  The caller should instantiate AES128,
 * AES192, or AES256 to create an AES block cipher with a specific
 * key size.
 *
 * \note This AES implementation does not have constant cache behaviour due
 * to the use of table lookups.  It may not be safe to use this implementation
 * in an environment where the attacker can observe the timing of encryption
 * and decryption operations.  Unless AES compatibility is required,
 * it is recommended that the ChaCha stream cipher be used instead.
 *
 * Reference: http://en.wikipedia.org/wiki/Advanced_Encryption_Standard
 *
 * \sa ChaCha, AES128, AES192, AES256
 */

/** @cond sbox */

// AES S-box (http://en.wikipedia.org/wiki/Rijndael_S-box)
static uint8_t const sbox[256] = {
    0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5,                                                  // 0x00
    0x30, 0x01, 0x67, 0x2B, 0xFE, 0xD7, 0xAB, 0x76, 0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0,  // 0x10
    0xAD, 0xD4, 0xA2, 0xAF, 0x9C, 0xA4, 0x72, 0xC0, 0xB7, 0xFD, 0x93, 0x26, 0x36, 0x3F, 0xF7, 0xCC,  // 0x20
    0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15, 0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A,  // 0x30
    0x07, 0x12, 0x80, 0xE2, 0xEB, 0x27, 0xB2, 0x75, 0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E, 0x5A, 0xA0,  // 0x40
    0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84, 0x53, 0xD1, 0x00, 0xED, 0x20, 0xFC, 0xB1, 0x5B,  // 0x50
    0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF, 0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85,  // 0x60
    0x45, 0xF9, 0x02, 0x7F, 0x50, 0x3C, 0x9F, 0xA8, 0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5,  // 0x70
    0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2, 0xCD, 0x0C, 0x13, 0xEC, 0x5F, 0x97, 0x44, 0x17,  // 0x80
    0xC4, 0xA7, 0x7E, 0x3D, 0x64, 0x5D, 0x19, 0x73, 0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88,  // 0x90
    0x46, 0xEE, 0xB8, 0x14, 0xDE, 0x5E, 0x0B, 0xDB, 0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C,  // 0xA0
    0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79, 0xE7, 0xC8, 0x37, 0x6D, 0x8D, 0xD5, 0x4E, 0xA9,  // 0xB0
    0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08, 0xBA, 0x78, 0x25, 0x2E, 0x1C, 0xA6, 0xB4, 0xC6,  // 0xC0
    0xE8, 0xDD, 0x74, 0x1F, 0x4B, 0xBD, 0x8B, 0x8A, 0x70, 0x3E, 0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E,  // 0xD0
    0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E, 0xE1, 0xF8, 0x98, 0x11, 0x69, 0xD9, 0x8E, 0x94,  // 0xE0
    0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF, 0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68,  // 0xF0
    0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16};

// AES inverse S-box (http://en.wikipedia.org/wiki/Rijndael_S-box)
static uint8_t const sbox_inverse[256] = {
    0x52, 0x09, 0x6A, 0xD5, 0x30, 0x36, 0xA5, 0x38,                                                  // 0x00
    0xBF, 0x40, 0xA3, 0x9E, 0x81, 0xF3, 0xD7, 0xFB, 0x7C, 0xE3, 0x39, 0x82, 0x9B, 0x2F, 0xFF, 0x87,  // 0x10
    0x34, 0x8E, 0x43, 0x44, 0xC4, 0xDE, 0xE9, 0xCB, 0x54, 0x7B, 0x94, 0x32, 0xA6, 0xC2, 0x23, 0x3D,  // 0x20
    0xEE, 0x4C, 0x95, 0x0B, 0x42, 0xFA, 0xC3, 0x4E, 0x08, 0x2E, 0xA1, 0x66, 0x28, 0xD9, 0x24, 0xB2,  // 0x30
    0x76, 0x5B, 0xA2, 0x49, 0x6D, 0x8B, 0xD1, 0x25, 0x72, 0xF8, 0xF6, 0x64, 0x86, 0x68, 0x98, 0x16,  // 0x40
    0xD4, 0xA4, 0x5C, 0xCC, 0x5D, 0x65, 0xB6, 0x92, 0x6C, 0x70, 0x48, 0x50, 0xFD, 0xED, 0xB9, 0xDA,  // 0x50
    0x5E, 0x15, 0x46, 0x57, 0xA7, 0x8D, 0x9D, 0x84, 0x90, 0xD8, 0xAB, 0x00, 0x8C, 0xBC, 0xD3, 0x0A,  // 0x60
    0xF7, 0xE4, 0x58, 0x05, 0xB8, 0xB3, 0x45, 0x06, 0xD0, 0x2C, 0x1E, 0x8F, 0xCA, 0x3F, 0x0F, 0x02,  // 0x70
    0xC1, 0xAF, 0xBD, 0x03, 0x01, 0x13, 0x8A, 0x6B, 0x3A, 0x91, 0x11, 0x41, 0x4F, 0x67, 0xDC, 0xEA,  // 0x80
    0x97, 0xF2, 0xCF, 0xCE, 0xF0, 0xB4, 0xE6, 0x73, 0x96, 0xAC, 0x74, 0x22, 0xE7, 0xAD, 0x35, 0x85,  // 0x90
    0xE2, 0xF9, 0x37, 0xE8, 0x1C, 0x75, 0xDF, 0x6E, 0x47, 0xF1, 0x1A, 0x71, 0x1D, 0x29, 0xC5, 0x89,  // 0xA0
    0x6F, 0xB7, 0x62, 0x0E, 0xAA, 0x18, 0xBE, 0x1B, 0xFC, 0x56, 0x3E, 0x4B, 0xC6, 0xD2, 0x79, 0x20,  // 0xB0
    0x9A, 0xDB, 0xC0, 0xFE, 0x78, 0xCD, 0x5A, 0xF4, 0x1F, 0xDD, 0xA8, 0x33, 0x88, 0x07, 0xC7, 0x31,  // 0xC0
    0xB1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xEC, 0x5F, 0x60, 0x51, 0x7F, 0xA9, 0x19, 0xB5, 0x4A, 0x0D,  // 0xD0
    0x2D, 0xE5, 0x7A, 0x9F, 0x93, 0xC9, 0x9C, 0xEF, 0xA0, 0xE0, 0x3B, 0x4D, 0xAE, 0x2A, 0xF5, 0xB0,  // 0xE0
    0xC8, 0xEB, 0xBB, 0x3C, 0x83, 0x53, 0x99, 0x61, 0x17, 0x2B, 0x04, 0x7E, 0xBA, 0x77, 0xD6, 0x26,  // 0xF0
    0xE1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0C, 0x7D};

/** @endcond */

/**
 * \brief Constructs an AES block cipher object.
 */
AESCommon::AESCommon() : rounds(0), schedule(0) {}

/**
 * \brief Destroys this AES block cipher object after clearing
 * sensitive information.
 */
AESCommon::~AESCommon() {}

/**
 * \brief Size of an AES block in bytes.
 * \return Always returns 16.
 */
size_t AESCommon::blockSize() const { return 16; }

// Constants to correct Galois multiplication for the high bits
// that are shifted out when multiplying by powers of two.
static uint8_t const K[8] = {0x00,
                             0x1B,
                             (0x1B << 1),
                             (0x1B << 1) ^ 0x1B,
                             (0x1B << 2),
                             (0x1B << 2) ^ 0x1B,
                             (0x1B << 2) ^ (0x1B << 1),
                             (0x1B << 2) ^ (0x1B << 1) ^ 0x1B};

// Multiply x by 2 in the Galois field, to achieve the effect of the following:
//
//     if (x & 0x80)
//         return (x << 1) ^ 0x1B;
//     else
//         return (x << 1);
//
// However, we don't want to use runtime conditionals if we can help it
// to avoid leaking timing information from the implementation.
// In this case, multiplication is slightly faster than table lookup on AVR.
#define gmul2(x) (t = ((uint16_t)(x)) << 1, ((uint8_t) t) ^ (uint8_t)(0x1B * ((uint8_t)(t >> 8))))

// Multiply x by 4 in the Galois field.
#define gmul4(x) (t = ((uint16_t)(x)) << 2, ((uint8_t) t) ^ K[t >> 8])

// Multiply x by 8 in the Galois field.
#define gmul8(x) (t = ((uint16_t)(x)) << 3, ((uint8_t) t) ^ K[t >> 8])

/** @cond aes_funcs */

void AESCommon::subBytesAndShiftRows(uint8_t *output, const uint8_t *input) {
  output[0 * 4 + 0] = sbox[input[0 * 4 + 0]];
  output[0 * 4 + 1] = sbox[input[1 * 4 + 1]];
  output[0 * 4 + 2] = sbox[input[2 * 4 + 2]];
  output[0 * 4 + 3] = sbox[input[3 * 4 + 3]];
  output[1 * 4 + 0] = sbox[input[1 * 4 + 0]];
  output[1 * 4 + 1] = sbox[input[2 * 4 + 1]];
  output[1 * 4 + 2] = sbox[input[3 * 4 + 2]];
  output[1 * 4 + 3] = sbox[input[0 * 4 + 3]];
  output[2 * 4 + 0] = sbox[input[2 * 4 + 0]];
  output[2 * 4 + 1] = sbox[input[3 * 4 + 1]];
  output[2 * 4 + 2] = sbox[input[0 * 4 + 2]];
  output[2 * 4 + 3] = sbox[input[1 * 4 + 3]];
  output[3 * 4 + 0] = sbox[input[3 * 4 + 0]];
  output[3 * 4 + 1] = sbox[input[0 * 4 + 1]];
  output[3 * 4 + 2] = sbox[input[1 * 4 + 2]];
  output[3 * 4 + 3] = sbox[input[2 * 4 + 3]];
}

void AESCommon::inverseShiftRowsAndSubBytes(uint8_t *output, const uint8_t *input) {
  output[0 * 4 + 0] = sbox_inverse[input[0 * 4 + 0]];
  output[0 * 4 + 1] = sbox_inverse[input[3 * 4 + 1]];
  output[0 * 4 + 2] = sbox_inverse[input[2 * 4 + 2]];
  output[0 * 4 + 3] = sbox_inverse[input[1 * 4 + 3]];
  output[1 * 4 + 0] = sbox_inverse[input[1 * 4 + 0]];
  output[1 * 4 + 1] = sbox_inverse[input[0 * 4 + 1]];
  output[1 * 4 + 2] = sbox_inverse[input[3 * 4 + 2]];
  output[1 * 4 + 3] = sbox_inverse[input[2 * 4 + 3]];
  output[2 * 4 + 0] = sbox_inverse[input[2 * 4 + 0]];
  output[2 * 4 + 1] = sbox_inverse[input[1 * 4 + 1]];
  output[2 * 4 + 2] = sbox_inverse[input[0 * 4 + 2]];
  output[2 * 4 + 3] = sbox_inverse[input[3 * 4 + 3]];
  output[3 * 4 + 0] = sbox_inverse[input[3 * 4 + 0]];
  output[3 * 4 + 1] = sbox_inverse[input[2 * 4 + 1]];
  output[3 * 4 + 2] = sbox_inverse[input[1 * 4 + 2]];
  output[3 * 4 + 3] = sbox_inverse[input[0 * 4 + 3]];
}

void AESCommon::mixColumn(uint8_t *output, uint8_t *input) {
  uint16_t t;  // Needed by the gmul2 macro.
  uint8_t a = input[0];
  uint8_t b = input[1];
  uint8_t c = input[2];
  uint8_t d = input[3];
  uint8_t a2 = gmul2(a);
  uint8_t b2 = gmul2(b);
  uint8_t c2 = gmul2(c);
  uint8_t d2 = gmul2(d);
  output[0] = a2 ^ b2 ^ b ^ c ^ d;
  output[1] = a ^ b2 ^ c2 ^ c ^ d;
  output[2] = a ^ b ^ c2 ^ d2 ^ d;
  output[3] = a2 ^ a ^ b ^ c ^ d2;
}

void AESCommon::inverseMixColumn(uint8_t *output, const uint8_t *input) {
  uint16_t t;  // Needed by the gmul2, gmul4, and gmul8 macros.
  uint8_t a = input[0];
  uint8_t b = input[1];
  uint8_t c = input[2];
  uint8_t d = input[3];
  uint8_t a2 = gmul2(a);
  uint8_t b2 = gmul2(b);
  uint8_t c2 = gmul2(c);
  uint8_t d2 = gmul2(d);
  uint8_t a4 = gmul4(a);
  uint8_t b4 = gmul4(b);
  uint8_t c4 = gmul4(c);
  uint8_t d4 = gmul4(d);
  uint8_t a8 = gmul8(a);
  uint8_t b8 = gmul8(b);
  uint8_t c8 = gmul8(c);
  uint8_t d8 = gmul8(d);
  output[0] = a8 ^ a4 ^ a2 ^ b8 ^ b2 ^ b ^ c8 ^ c4 ^ c ^ d8 ^ d;
  output[1] = a8 ^ a ^ b8 ^ b4 ^ b2 ^ c8 ^ c2 ^ c ^ d8 ^ d4 ^ d;
  output[2] = a8 ^ a4 ^ a ^ b8 ^ b ^ c8 ^ c4 ^ c2 ^ d8 ^ d2 ^ d;
  output[3] = a8 ^ a2 ^ a ^ b8 ^ b4 ^ b ^ c8 ^ c ^ d8 ^ d4 ^ d2;
}

/** @endcond */

void AESCommon::encryptBlock(uint8_t *output, const uint8_t *input) {
  const uint8_t *roundKey = schedule;
  uint8_t posn;
  uint8_t round;
  uint8_t state1[16];
  uint8_t state2[16];

  // Copy the input into the state and XOR with the first round key.
  for (posn = 0; posn < 16; ++posn)
    state1[posn] = input[posn] ^ roundKey[posn];
  roundKey += 16;

  // Perform all rounds except the last.
  for (round = rounds; round > 1; --round) {
    subBytesAndShiftRows(state2, state1);
    mixColumn(state1, state2);
    mixColumn(state1 + 4, state2 + 4);
    mixColumn(state1 + 8, state2 + 8);
    mixColumn(state1 + 12, state2 + 12);
    for (posn = 0; posn < 16; ++posn)
      state1[posn] ^= roundKey[posn];
    roundKey += 16;
  }

  // Perform the final round.
  subBytesAndShiftRows(state2, state1);
  for (posn = 0; posn < 16; ++posn)
    output[posn] = state2[posn] ^ roundKey[posn];
}

void AESCommon::decryptBlock(uint8_t *output, const uint8_t *input) {
  const uint8_t *roundKey = schedule + rounds * 16;
  uint8_t round;
  uint8_t posn;
  uint8_t state1[16];
  uint8_t state2[16];

  // Copy the input into the state and reverse the final round.
  for (posn = 0; posn < 16; ++posn)
    state1[posn] = input[posn] ^ roundKey[posn];
  inverseShiftRowsAndSubBytes(state2, state1);

  // Perform all other rounds in reverse.
  for (round = rounds; round > 1; --round) {
    roundKey -= 16;
    for (posn = 0; posn < 16; ++posn)
      state2[posn] ^= roundKey[posn];
    inverseMixColumn(state1, state2);
    inverseMixColumn(state1 + 4, state2 + 4);
    inverseMixColumn(state1 + 8, state2 + 8);
    inverseMixColumn(state1 + 12, state2 + 12);
    inverseShiftRowsAndSubBytes(state2, state1);
  }

  // Reverse the initial round and create the output words.
  roundKey -= 16;
  for (posn = 0; posn < 16; ++posn)
    output[posn] = state2[posn] ^ roundKey[posn];
}

void AESCommon::clear() { clean(schedule, (rounds + 1) * 16); }

/** @cond aes_keycore */

void AESCommon::keyScheduleCore(uint8_t *output, const uint8_t *input, uint8_t iteration) {
  // Rcon(i), 2^i in the Rijndael finite field, for i = 0..10.
  // http://en.wikipedia.org/wiki/Rijndael_key_schedule
  static uint8_t const rcon[11] = {0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40,  // 0x00
                                   0x80, 0x1B, 0x36};
  output[0] = (sbox[input[1]]) ^ (rcon[iteration]);
  output[1] = sbox[input[2]];
  output[2] = sbox[input[3]];
  output[3] = sbox[input[0]];
}

void AESCommon::applySbox(uint8_t *output, const uint8_t *input) {
  output[0] = sbox[input[0]];
  output[1] = sbox[input[1]];
  output[2] = sbox[input[2]];
  output[3] = sbox[input[3]];
}

/** @endcond */

#endif  // DSMR_CRYPTO_AES_DEFAULT
}  // namespace dsmr
}  // namespace esphome
