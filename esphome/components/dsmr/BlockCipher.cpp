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

#include "BlockCipher.h"

/**
 * \class BlockCipher BlockCipher.h <BlockCipher.h>
 * \brief Abstract base class for block ciphers.
 *
 * Block ciphers always operate in electronic codebook (ECB) mode.
 * Higher-level classes such as CFB128 and CTR128 wrap the block cipher to
 * create more useful classes for encryption and decryption of bulk data.
 *
 * References: http://en.wikipedia.org/wiki/Block_cipher,
 * http://en.wikipedia.org/wiki/Block_cipher_modes_of_operation#Electronic_codebook_.28ECB.29
 */

/**
 * \brief Constructs a block cipher.
 */
BlockCipher::BlockCipher()
{
}

/**
 * \brief Destroys this block cipher object.
 *
 * Subclasses are responsible for clearing temporary key schedules
 * and other buffers so as to avoid leaking sensitive information.
 *
 * \sa clear()
 */
BlockCipher::~BlockCipher()
{
}

/**
 * \fn size_t BlockCipher::blockSize() const
 * \brief Size of a single block processed by this cipher, in bytes.
 *
 * \return Returns the size of a block in bytes.
 *
 * \sa keySize(), encryptBlock()
 */

/**
 * \fn size_t BlockCipher::keySize() const
 * \brief Default size of the key for this block cipher, in bytes.
 *
 * This value indicates the default, or recommended, size for the key.
 *
 * \sa setKey(), blockSize()
 */

/**
 * \fn bool BlockCipher::setKey(const uint8_t *key, size_t len)
 * \brief Sets the key to use for future encryption and decryption operations.
 *
 * \param key The key to use.
 * \param len The length of the key.
 * \return Returns false if the key length is not supported, or the key
 * is somehow "weak" and unusable by this cipher.
 *
 * Use clear() or the destructor to remove the key and any other sensitive
 * data from the object once encryption or decryption is complete.
 *
 * \sa keySize(), clear()
 */

/**
 * \fn void BlockCipher::encryptBlock(uint8_t *output, const uint8_t *input)
 * \brief Encrypts a single block using this cipher.
 *
 * \param output The output buffer to put the ciphertext into.
 * Must be at least blockSize() bytes in length.
 * \param input The input buffer to read the plaintext from which is
 * allowed to overlap with \a output.  Must be at least blockSize()
 * bytes in length.
 *
 * \sa decryptBlock(), blockSize()
 */

/**
 * \fn void BlockCipher::decryptBlock(uint8_t *output, const uint8_t *input)
 * \brief Decrypts a single block using this cipher.
 *
 * \param output The output buffer to put the plaintext into.
 * Must be at least blockSize() bytes in length.
 * \param input The input buffer to read the ciphertext from which is
 * allowed to overlap with \a output.  Must be at least blockSize()
 * bytes in length.
 *
 * \sa encryptBlock(), blockSize()
 */

/**
 * \fn void BlockCipher::clear()
 * \brief Clears all security-sensitive state from this block cipher.
 *
 * Security-sensitive information includes key schedules and any
 * temporary state that is used by encryptBlock() or decryptBlock()
 * which is stored in the object itself.
 *
 * \sa setKey(), encryptBlock(), decryptBlock()
 */
