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

#include "GHASH.h"
#include "GF128.h"
#include "Crypto.h"
#include <cstring>
namespace esphome {
namespace dsmr {

/**
 * \class GHASH GHASH.h <GHASH.h>
 * \brief Implementation of the GHASH message authenticator.
 *
 * GHASH is the message authentication part of Galois Counter Mode (GCM).
 *
 * \note GHASH is not the same as GMAC.  GHASH implements the low level
 * hashing primitive that is used by both GCM and GMAC.  GMAC can be
 * simulated using GCM and an empty plaintext/ciphertext.
 *
 * References: <a href="http://csrc.nist.gov/publications/nistpubs/800-38D/SP-800-38D.pdf">NIST SP 800-38D</a>,
 * http://en.wikipedia.org/wiki/Galois/Counter_Mode
 *
 * \sa GCM
 */

/**
 * \brief Constructs a new GHASH message authenticator.
 */
GHASH::GHASH() { state.posn = 0; }

/**
 * \brief Destroys this GHASH message authenticator.
 */
GHASH::~GHASH() { clean(state); }

/**
 * \brief Resets the GHASH message authenticator for a new session.
 *
 * \param key Points to the 16 byte authentication key.
 *
 * \sa update(), finalize()
 */
void GHASH::reset(const void *key) {
  GF128::mulInit(state.H, key);
  memset(state.Y, 0, sizeof(state.Y));
  state.posn = 0;
}

/**
 * \brief Updates the message authenticator with more data.
 *
 * \param data Data to be hashed.
 * \param len Number of bytes of data to be hashed.
 *
 * If finalize() has already been called, then the behavior of update() will
 * be undefined.  Call reset() first to start a new authentication process.
 *
 * \sa pad(), reset(), finalize()
 */
void GHASH::update(const void *data, size_t len) {
  // XOR the input with state.Y in 128-bit chunks and process them.
  const uint8_t *d = (const uint8_t *) data;
  while (len > 0) {
    uint8_t size = 16 - state.posn;
    if (size > len)
      size = len;
    uint8_t *y = ((uint8_t *) state.Y) + state.posn;
    for (uint8_t i = 0; i < size; ++i)
      y[i] ^= d[i];
    state.posn += size;
    len -= size;
    d += size;
    if (state.posn == 16) {
      GF128::mul(state.Y, state.H);
      state.posn = 0;
    }
  }
}

/**
 * \brief Finalizes the authentication process and returns the token.
 *
 * \param token The buffer to return the token value in.
 * \param len The length of the \a token buffer between 0 and 16.
 *
 * If \a len is less than 16, then the token value will be truncated to
 * the first \a len bytes.  If \a len is greater than 16, then the remaining
 * bytes will left unchanged.
 *
 * If finalize() is called again, then the returned \a token value is
 * undefined.  Call reset() first to start a new authentication process.
 *
 * \sa reset(), update()
 */
void GHASH::finalize(void *token, size_t len) {
  // Pad with zeroes to a multiple of 16 bytes.
  pad();

  // The token is the current value of Y.
  if (len > 16)
    len = 16;
  memcpy(token, state.Y, len);
}

/**
 * \brief Pads the input stream with zero bytes to a multiple of 16.
 *
 * \sa update()
 */
void GHASH::pad() {
  if (state.posn != 0) {
    // Padding involves XOR'ing the rest of state.Y with zeroes,
    // which does nothing.  Immediately process the next chunk.
    GF128::mul(state.Y, state.H);
    state.posn = 0;
  }
}

/**
 * \brief Clears the authenticator's state, removing all sensitive data.
 */
void GHASH::clear() { clean(state); }

}  // namespace dsmr
}  // namespace esphome
