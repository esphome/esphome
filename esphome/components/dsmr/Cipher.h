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

#ifndef CRYPTO_CIPHER_h
#define CRYPTO_CIPHER_h

#include <inttypes.h>
#include <stddef.h>

class Cipher
{
public:
    Cipher();
    virtual ~Cipher();

    virtual size_t keySize() const = 0;
    virtual size_t ivSize() const = 0;

    virtual bool setKey(const uint8_t *key, size_t len) = 0;
    virtual bool setIV(const uint8_t *iv, size_t len) = 0;

    virtual void encrypt(uint8_t *output, const uint8_t *input, size_t len) = 0;
    virtual void decrypt(uint8_t *output, const uint8_t *input, size_t len) = 0;

    virtual void clear() = 0;
};

#endif
