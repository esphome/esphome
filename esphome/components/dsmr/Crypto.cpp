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

#include "Crypto.h"

/**
 * \brief Cleans a block of bytes.
 *
 * \param dest The destination block to be cleaned.
 * \param size The size of the destination to be cleaned in bytes.
 *
 * Unlike memset(), this function attempts to prevent the compiler
 * from optimizing away the clear on a memory buffer.
 */
void clean(void *dest, size_t size)
{
    // Force the use of volatile so that we actually clear the memory.
    // Otherwise the compiler might optimise the entire contents of this
    // function away, which will not be secure.
    volatile uint8_t *d = (volatile uint8_t *)dest;
    while (size > 0) {
        *d++ = 0;
        --size;
    }
}

/**
 * \fn void clean(T &var)
 * \brief Template function that cleans a variable.
 *
 * \param var A reference to the variable to clean.
 *
 * The variable will be cleared to all-zeroes in a secure manner.
 * Unlike memset(), this function attempts to prevent the compiler
 * from optimizing away the variable clear.
 */

/**
 * \brief Compares two memory blocks for equality.
 *
 * \param data1 Points to the first memory block.
 * \param data2 Points to the second memory block.
 * \param len The size of the memory blocks in bytes.
 *
 * Unlike memcmp(), this function attempts to compare the two memory blocks
 * in a way that will not reveal the contents in the instruction timing.
 * In particular, this function will not stop early if a byte is different.
 * It will instead continue onto the end of the array.
 */
bool secure_compare(const void *data1, const void *data2, size_t len)
{
    uint8_t result = 0;
    const uint8_t *d1 = (const uint8_t *)data1;
    const uint8_t *d2 = (const uint8_t *)data2;
    while (len > 0) {
        result |= (*d1++ ^ *d2++);
        --len;
    }
    return (bool)((((uint16_t)0x0100) - result) >> 8);
}

/**
 * \brief Calculates the CRC-8 value over an array in memory.
 *
 * \param tag Starting tag to distinguish this calculation.
 * \param data The data to checksum.
 * \param size The number of bytes to checksum.
 * \return The CRC-8 value over the data.
 *
 * This function does not provide any real security.  It is a simple
 * check that seed values have been initialized within EEPROM or Flash.
 * If the CRC-8 check fails, then it is assumed that the EEPROM/Flash
 * contents are invalid and should be re-initialized.
 *
 * Reference: http://www.sunshine2k.de/articles/coding/crc/understanding_crc.html#ch4
 */
uint8_t crypto_crc8(uint8_t tag, const void *data, unsigned size)
{
    const uint8_t *d = (const uint8_t *)data;
    uint8_t crc = 0xFF ^ tag;
    uint8_t bit;
    while (size > 0) {
        crc ^= *d++;
        for (bit = 0; bit < 8; ++bit) {
            // if (crc & 0x80)
            //     crc = (crc << 1) ^ 0x1D;
            // else
            //     crc = (crc << 1);
            uint8_t generator = (uint8_t)((((int8_t)crc) >> 7) & 0x1D);
            crc = (crc << 1) ^ generator;
        }
        --size;
    }
    return crc;
}
