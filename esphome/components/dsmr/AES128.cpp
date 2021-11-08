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

#if defined(DSMR_CRYPTO_AES_DEFAULT)

namespace esphome {
namespace dsmr {

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

}  // namespace dsmr
}  // namespace esphome

#endif  // DSMR_CRYPTO_AES_DEFAULT
