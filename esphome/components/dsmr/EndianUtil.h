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

#ifndef CRYPTO_ENDIANUTIL_H
#define CRYPTO_ENDIANUTIL_H

#include <inttypes.h>

#if !defined(HOST_BUILD)

// CPU is assumed to be little endian.   Edit this file if you
// need to port this library to a big endian CPU.

#define CRYPTO_LITTLE_ENDIAN 1

#define htole16(x)  (x)
#define le16toh(x)  (x)
#define htobe16(x)  \
    (__extension__ ({ \
        uint16_t _temp = (x); \
        ((_temp >> 8) & 0x00FF) | \
        ((_temp << 8) & 0xFF00); \
    }))
#define be16toh(x)  (htobe16((x)))

#define htole32(x)  (x)
#define le32toh(x)  (x)
#define htobe32(x)  \
    (__extension__ ({ \
        uint32_t _temp = (x); \
        ((_temp >> 24) & 0x000000FF) | \
        ((_temp >>  8) & 0x0000FF00) | \
        ((_temp <<  8) & 0x00FF0000) | \
        ((_temp << 24) & 0xFF000000); \
    }))
#define be32toh(x)  (htobe32((x)))

#define htole64(x)  (x)
#define le64toh(x)  (x)
#define htobe64(x)  \
    (__extension__ ({ \
        uint64_t __temp = (x); \
        uint32_t __low = htobe32((uint32_t)__temp); \
        uint32_t __high = htobe32((uint32_t)(__temp >> 32)); \
        (((uint64_t)__low) << 32) | __high; \
    }))
#define be64toh(x)  (htobe64((x)))

#else // HOST_BUILD

#include <endian.h>
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define CRYPTO_LITTLE_ENDIAN 1
#endif

#endif // HOST_BUILD

#endif
