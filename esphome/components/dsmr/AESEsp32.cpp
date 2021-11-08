/*
 * Copyright (C) 2018 Southern Storm Software, Pty Ltd.
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
namespace esphome {
namespace dsmr {

// AES implementation for ESP32 using the hardware crypto module.

#if defined(CRYPTO_AES_ESP32)

AESCommon::AESCommon(uint8_t keySize) { ctx.key_bytes = keySize; }

AESCommon::~AESCommon() { clean(ctx.key, sizeof(ctx.key)); }

size_t AESCommon::blockSize() const { return 16; }

size_t AESCommon::keySize() const { return ctx.key_bytes; }

bool AESCommon::setKey(const uint8_t *key, size_t len) {
  if (len == ctx.key_bytes) {
    // Do the effect of esp_aes_setkey() which is just a memcpy().
    memcpy(ctx.key, key, len);
    return true;
  }
  return false;
}

void AESCommon::encryptBlock(uint8_t *output, const uint8_t *input) { esp_aes_encrypt(&ctx, input, output); }

void AESCommon::decryptBlock(uint8_t *output, const uint8_t *input) { esp_aes_decrypt(&ctx, input, output); }

void AESCommon::clear() { clean(ctx.key, sizeof(ctx.key)); }

AES128::~AES128() {}

AES192::~AES192() {}

AES256::~AES256() {}

#endif  // CRYPTO_AES_ESP32
}  // namespace dsmr
}  // namespace esphome
