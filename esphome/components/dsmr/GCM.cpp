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

#include "GCM.h"
#include "Crypto.h"
#include "EndianUtil.h"
#include <cstring>

/**
 * \class GCMCommon GCM.h <GCM.h>
 * \brief Concrete base class to assist with implementing GCM for
 * 128-bit block ciphers.
 *
 * References: <a href="http://csrc.nist.gov/publications/nistpubs/800-38D/SP-800-38D.pdf">NIST SP 800-38D</a>,
 * http://en.wikipedia.org/wiki/Galois/Counter_Mode
 *
 * \sa GCM
 */

/**
 * \brief Constructs a new cipher in GCM mode.
 *
 * This constructor must be followed by a call to setBlockCipher().
 */
GCMCommon::GCMCommon() : blockCipher(0) {
  state.authSize = 0;
  state.dataSize = 0;
  state.dataStarted = false;
  state.posn = 16;
}

/**
 * \brief Destroys this cipher object after clearing sensitive information.
 */
GCMCommon::~GCMCommon() { clean(state); }

size_t GCMCommon::keySize() const { return blockCipher->keySize(); }

size_t GCMCommon::ivSize() const {
  // The GCM specification recommends an IV size of 96 bits.
  return 12;
}

size_t GCMCommon::tagSize() const { return 16; }

bool GCMCommon::setKey(const uint8_t *key, size_t len) {
  // Set the encryption key for the block cipher.
  return blockCipher->setKey(key, len);
}

bool GCMCommon::setIV(const uint8_t *iv, size_t len) {
  // Format the counter block from the IV.
  if (len == 12) {
    // IV's of exactly 96 bits are used directly as the counter block.
    memcpy(state.counter, iv, 12);
    state.counter[12] = 0;
    state.counter[13] = 0;
    state.counter[14] = 0;
    state.counter[15] = 1;
  } else {
    // IV's of other sizes are hashed to produce the counter block.
    memset(state.nonce, 0, 16);
    blockCipher->encryptBlock(state.nonce, state.nonce);
    ghash.reset(state.nonce);
    ghash.update(iv, len);
    ghash.pad();
    uint64_t sizes[2] = {0, htobe64(((uint64_t) len) * 8)};
    ghash.update(sizes, sizeof(sizes));
    clean(sizes);
    ghash.finalize(state.counter, 16);
  }

  // Reset the GCM object ready to process auth or payload data.
  state.authSize = 0;
  state.dataSize = 0;
  state.dataStarted = false;
  state.posn = 16;

  // Construct the hashing key by encrypting a zero block.
  memset(state.nonce, 0, 16);
  blockCipher->encryptBlock(state.nonce, state.nonce);
  ghash.reset(state.nonce);

  // Replace the hash key in "nonce" with the encrypted counter.
  // This value will be XOR'ed with the final authentication hash
  // value in computeTag().
  blockCipher->encryptBlock(state.nonce, state.counter);
  return true;
}

/**
 * \brief Increments the least significant 32 bits of the counter block.
 *
 * \param counter The counter block to increment.
 */
static inline void increment(uint8_t counter[16]) {
  uint16_t carry = 1;
  carry += counter[15];
  counter[15] = (uint8_t) carry;
  carry = (carry >> 8) + counter[14];
  counter[14] = (uint8_t) carry;
  carry = (carry >> 8) + counter[13];
  counter[13] = (uint8_t) carry;
  carry = (carry >> 8) + counter[12];
  counter[12] = (uint8_t) carry;
}

void GCMCommon::encrypt(uint8_t *output, const uint8_t *input, size_t len) {
  // Finalize the authenticated data if necessary.
  if (!state.dataStarted) {
    ghash.pad();
    state.dataStarted = true;
  }

  // Encrypt the plaintext using the block cipher in counter mode.
  uint8_t *out = output;
  size_t size = len;
  while (size > 0) {
    // Create a new keystream block if necessary.
    if (state.posn >= 16) {
      increment(state.counter);
      blockCipher->encryptBlock(state.stream, state.counter);
      state.posn = 0;
    }

    // Encrypt as many bytes as we can using the keystream block.
    uint8_t temp = 16 - state.posn;
    if (temp > size)
      temp = size;
    uint8_t *stream = state.stream + state.posn;
    state.posn += temp;
    size -= temp;
    while (temp > 0) {
      *out++ = *input++ ^ *stream++;
      --temp;
    }
  }

  // Feed the ciphertext into the hash.
  ghash.update(output, len);
  state.dataSize += len;
}

void GCMCommon::decrypt(uint8_t *output, const uint8_t *input, size_t len) {
  // Finalize the authenticated data if necessary.
  if (!state.dataStarted) {
    ghash.pad();
    state.dataStarted = true;
  }

  // Feed the ciphertext into the hash before we decrypt it.
  ghash.update(input, len);
  state.dataSize += len;

  // Decrypt the plaintext using the block cipher in counter mode.
  while (len > 0) {
    // Create a new keystream block if necessary.
    if (state.posn >= 16) {
      increment(state.counter);
      blockCipher->encryptBlock(state.stream, state.counter);
      state.posn = 0;
    }

    // Decrypt as many bytes as we can using the keystream block.
    uint8_t temp = 16 - state.posn;
    if (temp > len)
      temp = len;
    uint8_t *stream = state.stream + state.posn;
    state.posn += temp;
    len -= temp;
    while (temp > 0) {
      *output++ = *input++ ^ *stream++;
      --temp;
    }
  }
}

void GCMCommon::addAuthData(const void *data, size_t len) {
  if (!state.dataStarted) {
    ghash.update(data, len);
    state.authSize += len;
  }
}

void GCMCommon::computeTag(void *tag, size_t len) {
  // Pad the hashed data and add the sizes.
  ghash.pad();
  uint64_t sizes[2] = {htobe64(state.authSize * 8), htobe64(state.dataSize * 8)};
  ghash.update(sizes, sizeof(sizes));
  clean(sizes);

  // Get the finalized hash, encrypt it with the nonce, and return the tag.
  ghash.finalize(state.stream, 16);
  for (uint8_t posn = 0; posn < 16; ++posn)
    state.stream[posn] ^= state.nonce[posn];
  if (len > 16)
    len = 16;
  memcpy(tag, state.stream, len);
}

bool GCMCommon::checkTag(const void *tag, size_t len) {
  // Can never match if the expected tag length is too long.
  if (len > 16)
    return false;

  // Compute the tag and check it.
  computeTag(state.counter, 16);
  return secure_compare(state.counter, tag, len);
}

void GCMCommon::clear() {
  blockCipher->clear();
  ghash.clear();
  clean(state);
  state.posn = 16;
}

/**
 * \fn void GCMCommon::setBlockCipher(BlockCipher *cipher)
 * \brief Sets the block cipher to use for this GCM object.
 *
 * \param cipher The block cipher to use to implement GCM mode.
 * This object must have a block size of 128 bits (16 bytes).
 */

/**
 * \class GCM GCM.h <GCM.h>
 * \brief Implementation of the Galois Counter Mode (GCM).
 *
 * GCM mode converts a block cipher into an authenticated cipher
 * that uses the block cipher T to encrypt and GHASH to authenticate.
 *
 * The size of the key is determined by the underlying block cipher T.
 * The IV is recommended to be 96 bits (12 bytes) in length, but other
 * lengths are supported as well.  The default tagSize() is 128 bits
 * (16 bytes) but the GCM specification does allow other tag sizes:
 * 32, 64, 96, 104, 112, 120, or 128 bits (4, 8, 12, 13, 14, 15, or 16 bytes).
 *
 * The template parameter T must be a concrete subclass of BlockCipher
 * indicating the specific block cipher to use.  The block cipher must
 * have a block size of 128 bits.  For example, the following creates a
 * GCM object using AES256 as the underlying cipher and then uses it
 * to encrypt and authenticate a \c plaintext block:
 *
 * \code
 * GCM<AES256> gcm;
 * gcm.setKey(key, sizeof(key));
 * gcm.setIV(iv, sizeof(iv));
 * gcm.addAuthData(adata, sizeof(adata));
 * gcm.encrypt(ciphertext, plaintext, sizeof(plaintext));
 * gcm.computeTag(tag, sizeof(tag));
 * \endcode
 *
 * The decryption process is almost identical to convert a \c ciphertext and
 * \a tag back into plaintext and then check the tag:
 *
 * \code
 * GCM<AES256> gcm;
 * gcm.setKey(key, sizeof(key));
 * gcm.setIV(iv, sizeof(iv));
 * gcm.addAuthData(adata, sizeof(adata));
 * gcm.decrypt(plaintext, ciphertext, sizeof(ciphertext));
 * if (!gcm.checkTag(tag, sizeof(tag))) {
 *     // The data was invalid - do not use it.
 *     ...
 * }
 * \endcode
 *
 * The GCM class can also be used to implement GMAC message authentication
 * by omitting the plaintext:
 *
 * \code
 * GCM<AES256> gcm;
 * gcm.setKey(key, sizeof(key));
 * gcm.setIV(iv, sizeof(iv));
 * gcm.addAuthData(adata1, sizeof(adata1));
 * gcm.addAuthData(adata2, sizeof(adata1));
 * ...
 * gcm.addAuthData(adataN, sizeof(adataN));
 * gcm.computeTag(tag, sizeof(tag));
 * \endcode
 *
 * References: <a href="http://csrc.nist.gov/publications/nistpubs/800-38D/SP-800-38D.pdf">NIST SP 800-38D</a>,
 * http://en.wikipedia.org/wiki/Galois/Counter_Mode
 *
 * \sa GCMCommon, GHASH
 */

/**
 * \fn GCM::GCM()
 * \brief Constructs a new GCM object for the block cipher T.
 */