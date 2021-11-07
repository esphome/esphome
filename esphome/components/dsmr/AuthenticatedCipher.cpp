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

#include "AuthenticatedCipher.h"

/**
 * \class AuthenticatedCipher AuthenticatedCipher.h <AuthenticatedCipher.h>
 * \brief Abstract base class for authenticated ciphers.
 *
 * This class abstracts the details of algorithms that provide Authenticated
 * Encryption with Associated Data (AEAD).  Such algorithms combine
 * encryption with message authentication to provide a single primitive.
 *
 * Authenticated ciphers have four parameters: the secret key, an
 * initialization vector (called a "nonce" in the literature), the
 * plaintext, and some associated data which is to be authenticated
 * with the plaintext but not encrypted.  Associated data might be
 * sequence numbers, IP addresses, protocol versions, or other information
 * that is not secret but is important and unique to the session.
 *
 * Subclasses encrypt the plaintext content and output the ciphertext.
 * Once all plaintext has been processed, the caller should invoke
 * computeTag() to obtain the authentication tag to transmit with
 * the ciphertext.  When the ciphertext is later decrypted, the checkTag()
 * function can be used to check that the data is authentic.
 *
 * Reference: <a href="http://tools.ietf.org/html/rfc5116">RFC 5116</a>
 *
 * \sa Cipher
 */

/**
 * \brief Constructs a new authenticated cipher.
 */
AuthenticatedCipher::AuthenticatedCipher()
{
}

/**
 * \brief Destroys this authenticated cipher.
 */
AuthenticatedCipher::~AuthenticatedCipher()
{
}

/**
 * \fn size_t AuthenticatedCipher::tagSize() const
 * \brief Returns the size of the authentication tag.
 *
 * \return The size of the authentication tag in bytes.
 *
 * By default this function should return the largest tag size supported
 * by the authenticated cipher.
 *
 * \sa computeTag()
 */

/**
 * \fn void AuthenticatedCipher::addAuthData(const void *data, size_t len)
 * \brief Adds extra data that will be authenticated but not encrypted.
 *
 * \param data The extra data to be authenticated.
 * \param len The number of bytes of extra data to be authenticated.
 *
 * This function must be called before the first call to encrypt() or
 * decrypt().  That is, it is assumed that all extra data for authentication
 * is available before the first payload data block and that it will be
 * prepended to the payload for authentication.  If the subclass needs to
 * process the extra data after the payload, then it is responsible for saving
 * \a data away until it is needed during computeTag() or checkTag().
 *
 * This function can be called multiple times with separate extra data
 * blocks for authentication.  All such data will be concatenated into a
 * single block for authentication purposes.
 */

/**
 * \fn void AuthenticatedCipher::computeTag(void *tag, size_t len)
 * \brief Finalizes the encryption process and computes the authentication tag.
 *
 * \param tag Points to the buffer to write the tag to.
 * \param len The length of the tag, which may be less than tagSize() to
 * truncate the tag to the first \a len bytes.
 *
 * \sa checkTag()
 */

/**
 * \fn bool AuthenticatedCipher::checkTag(const void *tag, size_t len)
 * \brief Finalizes the decryption process and checks the authentication tag.
 *
 * \param tag The tag value from the incoming ciphertext to be checked.
 * \param len The length of the tag value in bytes, which may be less
 * than tagSize().
 *
 * \return Returns true if the \a tag is identical to the first \a len
 * bytes of the authentication tag that was calculated during the
 * decryption process.  Returns false otherwise.
 *
 * This function must be called after the final block of ciphertext is
 * passed to decrypt() to determine if the data could be authenticated.
 *
 * \note Authenticated cipher modes usually require that if the tag could
 * not be verified, then all of the data that was previously decrypted
 * <i>must</i> be discarded.  It is unwise to use the decrypted data for
 * any purpose before it can be verified.  Callers are responsible for
 * ensuring that any data returned via previous calls to decrypt() is
 * discarded if checkTag() returns false.
 *
 * \sa computeTag()
 */
