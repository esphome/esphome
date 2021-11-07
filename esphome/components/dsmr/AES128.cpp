/*
 * Copyright (C) 2015,2018 Southern Storm Software, Pty Ltd.
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
#include <cstring>

#if defined(CRYPTO_AES_DEFAULT) || defined(CRYPTO_DOC)

/**
 * \class AES128 AES.h <AES.h>
 * \brief AES block cipher with 128-bit keys.
 *
 * \sa AES192, AES256, AESTiny128, AESSmall128
 */

/**
 * \brief Constructs an AES 128-bit block cipher with no initial key.
 *
 * This constructor must be followed by a call to setKey() before the
 * block cipher can be used for encryption or decryption.
 */
AES128::AES128() {
  rounds = 10;
  schedule = sched;
}

AES128::~AES128() { clean(sched); }

/**
 * \brief Size of a 128-bit AES key in bytes.
 * \return Always returns 16.
 */
size_t AES128::keySize() const { return 16; }

bool AES128::setKey(const uint8_t *key, size_t len) {
  if (len != 16)
    return false;

  // Copy the key itself into the first 16 bytes of the schedule.
  uint8_t *schedule = sched;
  memcpy(schedule, key, 16);

  // Expand the key schedule until we have 176 bytes of expanded key.
  uint8_t iteration = 1;
  uint8_t n = 16;
  uint8_t w = 4;
  while (n < 176) {
    if (w == 4) {
      // Every 16 bytes (4 words) we need to apply the key schedule core.
      keyScheduleCore(schedule + 16, schedule + 12, iteration);
      schedule[16] ^= schedule[0];
      schedule[17] ^= schedule[1];
      schedule[18] ^= schedule[2];
      schedule[19] ^= schedule[3];
      ++iteration;
      w = 0;
    } else {
      // Otherwise just XOR the word with the one 16 bytes previous.
      schedule[16] = schedule[12] ^ schedule[0];
      schedule[17] = schedule[13] ^ schedule[1];
      schedule[18] = schedule[14] ^ schedule[2];
      schedule[19] = schedule[15] ^ schedule[3];
    }

    // Advance to the next word in the schedule.
    schedule += 4;
    n += 4;
    ++w;
  }

  return true;
}

/**
 * \class AESTiny128 AES.h <AES.h>
 * \brief AES block cipher with 128-bit keys and tiny memory usage.
 *
 * This class differs from the AES128 class in the following ways:
 *
 * \li RAM requirements are vastly reduced.  The key is stored directly
 * and then expanded to the full key schedule round by round.  The setKey()
 * method is very fast because of this.
 * \li Performance of encryptBlock() is slower than for AES128 due to
 * expanding the key on the fly rather than ahead of time.
 * \li The decryptBlock() function is not supported, which means that CBC
 * mode cannot be used but the CTR, CFB, OFB, EAX, and GCM modes can be used.
 *
 * This class is useful when RAM is at a premium, CBC mode is not required,
 * and reduced encryption performance is not a hindrance to the application.
 *
 * The companion AESSmall128 class supports decryptBlock() at the cost of
 * some additional memory and slower setKey() times.
 *
 * \sa AESSmall128, AES128
 */

/** @cond */

// Helper macros.
#define KCORE(n) \
  do { \
    AESCommon::keyScheduleCore(temp, schedule + 12, (n)); \
    schedule[0] ^= temp[0]; \
    schedule[1] ^= temp[1]; \
    schedule[2] ^= temp[2]; \
    schedule[3] ^= temp[3]; \
  } while (0)
#define KXOR(a, b) \
  do { \
    schedule[(a) *4] ^= schedule[(b) *4]; \
    schedule[(a) *4 + 1] ^= schedule[(b) *4 + 1]; \
    schedule[(a) *4 + 2] ^= schedule[(b) *4 + 2]; \
    schedule[(a) *4 + 3] ^= schedule[(b) *4 + 3]; \
  } while (0)

/** @endcond */

/**
 * \brief Constructs an AES 128-bit block cipher with no initial key.
 *
 * This constructor must be followed by a call to setKey() before the
 * block cipher can be used for encryption or decryption.
 */
AESTiny128::AESTiny128() {}

AESTiny128::~AESTiny128() { clean(schedule); }

/**
 * \brief Size of an AES block in bytes.
 * \return Always returns 16.
 */
size_t AESTiny128::blockSize() const { return 16; }

/**
 * \brief Size of a 128-bit AES key in bytes.
 * \return Always returns 16.
 */
size_t AESTiny128::keySize() const { return 16; }

bool AESTiny128::setKey(const uint8_t *key, size_t len) {
  if (len == 16) {
    // Make a copy of the key - it will be expanded in encryptBlock().
    memcpy(schedule, key, 16);
    return true;
  }
  return false;
}

void AESTiny128::encryptBlock(uint8_t *output, const uint8_t *input) {
  uint8_t schedule[16];
  uint8_t posn;
  uint8_t round;
  uint8_t state1[16];
  uint8_t state2[16];
  uint8_t temp[4];

  // Start with the key in the schedule buffer.
  memcpy(schedule, this->schedule, 16);

  // Copy the input into the state and XOR with the key schedule.
  for (posn = 0; posn < 16; ++posn)
    state1[posn] = input[posn] ^ schedule[posn];

  // Perform the first 9 rounds of the cipher.
  for (round = 1; round <= 9; ++round) {
    // Expand the next 16 bytes of the key schedule.
    KCORE(round);
    KXOR(1, 0);
    KXOR(2, 1);
    KXOR(3, 2);

    // Encrypt using the key schedule.
    AESCommon::subBytesAndShiftRows(state2, state1);
    AESCommon::mixColumn(state1, state2);
    AESCommon::mixColumn(state1 + 4, state2 + 4);
    AESCommon::mixColumn(state1 + 8, state2 + 8);
    AESCommon::mixColumn(state1 + 12, state2 + 12);
    for (posn = 0; posn < 16; ++posn)
      state1[posn] ^= schedule[posn];
  }

  // Expand the final 16 bytes of the key schedule.
  KCORE(10);
  KXOR(1, 0);
  KXOR(2, 1);
  KXOR(3, 2);

  // Perform the final round.
  AESCommon::subBytesAndShiftRows(state2, state1);
  for (posn = 0; posn < 16; ++posn)
    output[posn] = state2[posn] ^ schedule[posn];
}

void AESTiny128::decryptBlock(uint8_t *output, const uint8_t *input) {
  // Decryption is not supported by AESTiny128.
}

void AESTiny128::clear() { clean(schedule); }

/**
 * \class AESSmall128 AES.h <AES.h>
 * \brief AES block cipher with 128-bit keys and reduced memory usage.
 *
 * This class differs from the AES128 class in that the RAM requirements are
 * vastly reduced.  The key schedule is expanded round by round instead of
 * being generated and stored by setKey().  The performance of encryption
 * and decryption is slightly less because of this.
 *
 * This class is useful when RAM is at a premium and reduced encryption
 * performance is not a hindrance to the application.
 *
 * The companion AESTiny128 class uses even less RAM but only supports the
 * encryptBlock() operation.  Block cipher modes like CTR, EAX, and GCM
 * do not need the decryptBlock() operation, so AESTiny128 may be a better
 * option than AESSmall128 for many applications.
 *
 * \sa AESTiny128, AES128
 */

/**
 * \brief Constructs an AES 128-bit block cipher with no initial key.
 *
 * This constructor must be followed by a call to setKey() before the
 * block cipher can be used for encryption or decryption.
 */
AESSmall128::AESSmall128() {}

AESSmall128::~AESSmall128() { clean(reverse); }

bool AESSmall128::setKey(const uint8_t *key, size_t len) {
  uint8_t *schedule;
  uint8_t round;
  uint8_t temp[4];

  // Set the encryption key first.
  if (!AESTiny128::setKey(key, len))
    return false;

  // Expand the key schedule up to the last round which gives
  // us the round keys to use for the final two rounds.  We can
  // then work backwards from there in decryptBlock().
  schedule = reverse;
  memcpy(schedule, key, 16);
  for (round = 1; round <= 10; ++round) {
    KCORE(round);
    KXOR(1, 0);
    KXOR(2, 1);
    KXOR(3, 2);
  }

  // Key is ready to go.
  return true;
}

void AESSmall128::decryptBlock(uint8_t *output, const uint8_t *input) {
  uint8_t schedule[16];
  uint8_t round;
  uint8_t posn;
  uint8_t state1[16];
  uint8_t state2[16];
  uint8_t temp[4];

  // Start with the end of the decryption schedule.
  memcpy(schedule, reverse, 16);

  // Copy the input into the state and reverse the final round.
  for (posn = 0; posn < 16; ++posn)
    state1[posn] = input[posn] ^ schedule[posn];
  AESCommon::inverseShiftRowsAndSubBytes(state2, state1);
  KXOR(3, 2);
  KXOR(2, 1);
  KXOR(1, 0);
  KCORE(10);

  // Perform the next 9 rounds of the decryption process.
  for (round = 9; round >= 1; --round) {
    // Decrypt using the key schedule.
    for (posn = 0; posn < 16; ++posn)
      state2[posn] ^= schedule[posn];
    AESCommon::inverseMixColumn(state1, state2);
    AESCommon::inverseMixColumn(state1 + 4, state2 + 4);
    AESCommon::inverseMixColumn(state1 + 8, state2 + 8);
    AESCommon::inverseMixColumn(state1 + 12, state2 + 12);
    AESCommon::inverseShiftRowsAndSubBytes(state2, state1);

    // Expand the next 16 bytes of the key schedule in reverse.
    KXOR(3, 2);
    KXOR(2, 1);
    KXOR(1, 0);
    KCORE(round);
  }

  // Reverse the initial round and create the output words.
  for (posn = 0; posn < 16; ++posn)
    output[posn] = state2[posn] ^ schedule[posn];
}

void AESSmall128::clear() {
  clean(reverse);
  AESTiny128::clear();
}

#endif  // CRYPTO_AES_DEFAULT
