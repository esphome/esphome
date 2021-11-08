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

#pragma once

#include "BlockCipher.h"

// Determine which AES implementation to export to applications.
#if defined(ESP32)
#define DSMR_CRYPTO_AES_ESP32 1
#else
#define DSMR_CRYPTO_AES_DEFAULT 1
#endif

namespace esphome {
namespace dsmr {
#if defined(DSMR_CRYPTO_AES_DEFAULT)

class AESCommon : public BlockCipher {
 public:
  virtual ~AESCommon();

  size_t blockSize() const;

  void encryptBlock(uint8_t *output, const uint8_t *input);
  void decryptBlock(uint8_t *output, const uint8_t *input);

  void clear();

 protected:
  AESCommon();

  /** @cond aes_internal */
  uint8_t rounds;
  uint8_t *schedule;

  static void subBytesAndShiftRows(uint8_t *output, const uint8_t *input);
  static void inverseShiftRowsAndSubBytes(uint8_t *output, const uint8_t *input);
  static void mixColumn(uint8_t *output, uint8_t *input);
  static void inverseMixColumn(uint8_t *output, const uint8_t *input);
  static void keyScheduleCore(uint8_t *output, const uint8_t *input, uint8_t iteration);
  static void applySbox(uint8_t *output, const uint8_t *input);
  /** @endcond */
};

class AES128 : public AESCommon {
 public:
  AES128();
  virtual ~AES128();

  size_t keySize() const;

  bool setKey(const uint8_t *key, size_t len);

 private:
  uint8_t sched[176];
};

#endif  // DSMR_CRYPTO_AES_DEFAULT

#if defined(DSMR_CRYPTO_AES_ESP32)

class AESCommon : public BlockCipher {
 public:
  virtual ~AESCommon();

  size_t blockSize() const;
  size_t keySize() const;

  bool setKey(const uint8_t *key, size_t len);

  void encryptBlock(uint8_t *output, const uint8_t *input);
  void decryptBlock(uint8_t *output, const uint8_t *input);

  void clear();

 protected:
  AESCommon(uint8_t keySize);

 private:
  esp_aes_context ctx;
};

class AES128 : public AESCommon {
 public:
  AES128() : AESCommon(16) {}
  virtual ~AES128();
};

#endif  // DSMR_CRYPTO_AES_ESP32
}  // namespace dsmr
}  // namespace esphome
