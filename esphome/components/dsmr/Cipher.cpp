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

#include "Cipher.h"

/**
 * \class Cipher Cipher.h <Cipher.h>
 * \brief Abstract base class for stream ciphers.
 *
 * This class is intended for implementing ciphers that operate on arbitrary
 * amounts of data.  In particular, stream ciphers where the number of
 * bytes that are input to encrypt() or decrypt() is exactly the same as
 * the number of bytes that are output.
 *
 * All of the stream ciphers such as ChaCha inherit directly from this class,
 * together with block cipher modes such as CTR and CFB.
 */

/**
 * \brief Constructs a new cipher object.
 */
Cipher::Cipher()
{
}

/**
 * \brief Destroys this cipher object.
 *
 * Subclasses are responsible for clearing temporary key schedules
 * and other buffers so as to avoid leaking sensitive information.
 *
 * \sa clear()
 */
Cipher::~Cipher()
{
}

/**
 * \fn size_t Cipher::keySize() const
 * \brief Default size of the key for this cipher, in bytes.
 *
 * If the cipher supports variable-sized keys, keySize() indicates the
 * default or recommended key size.  The cipher may support other key sizes.
 *
 * \sa setKey(), ivSize()
 */

/**
 * \fn size_t Cipher::ivSize() const
 * \brief Size of the initialization vector for this cipher, in bytes.
 *
 * If the cipher does not need an initialization vector, this function
 * will return zero.
 */

/**
 * \fn bool Cipher::setKey(const uint8_t *key, size_t len)
 * \brief Sets the key to use for future encryption and decryption operations.
 *
 * \param key The key to use.
 * \param len The length of the key in bytes.
 * \return Returns false if the key length is not supported, or the key
 * is somehow "weak" and unusable by this cipher.
 *
 * Use clear() or the destructor to remove the key and any other sensitive
 * data from the object once encryption or decryption is complete.
 *
 * Calling setKey() resets the cipher.  Any temporary data that was being
 * retained for encrypting partial blocks will be abandoned.
 *
 * \sa keySize(), clear()
 */

/**
 * \fn bool Cipher::setIV(const uint8_t *iv, size_t len)
 * \brief Sets the initialization vector to use for future encryption and
 * decryption operations.
 *
 * \param iv The initialization vector to use.
 * \param len The length of the initialization vector in bytes.
 * \return Returns false if the length is not supported.
 *
 * Initialization vectors should be set before the first call to
 * encrypt() or decrypt() after a setKey() call.  If the initialization
 * vector is changed after encryption or decryption begins,
 * then the behaviour is undefined.
 *
 * \note The IV is not encoded into the output stream by encrypt().
 * The caller is responsible for communicating the IV to the other party.
 *
 * \sa ivSize()
 */

/**
 * \fn void Cipher::encrypt(uint8_t *output, const uint8_t *input, size_t len)
 * \brief Encrypts an input buffer and writes the ciphertext to an
 * output buffer.
 *
 * \param output The output buffer to write to, which may be the same
 * buffer as \a input.  The \a output buffer must have at least as many
 * bytes as the \a input buffer.
 * \param input The input buffer to read from.
 * \param len The number of bytes to encrypt.
 *
 * The encrypt() function can be called multiple times with different
 * regions of the plaintext data.
 *
 * \sa decrypt()
 */

/**
 * \fn void Cipher::decrypt(uint8_t *output, const uint8_t *input, size_t len)
 * \brief Decrypts an input buffer and writes the plaintext to an
 * output buffer.
 *
 * \param output The output buffer to write to, which may be the same
 * buffer as \a input.  The \a output buffer must have at least as many
 * bytes as the \a input buffer.
 * \param input The input buffer to read from.
 * \param len The number of bytes to decrypt.
 *
 * The decrypt() function can be called multiple times with different
 * regions of the ciphertext data.
 *
 * \sa encrypt()
 */

/**
 * \fn void Cipher::clear()
 * \brief Clears all security-sensitive state from this cipher.
 *
 * Security-sensitive information includes key schedules, initialization
 * vectors, and any temporary state that is used by encrypt() or decrypt()
 * which is stored in the cipher itself.
 */
