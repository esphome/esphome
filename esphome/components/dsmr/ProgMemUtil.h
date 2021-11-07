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

#ifndef CRYPTO_PROGMEMUTIL_H
#define CRYPTO_PROGMEMUTIL_H

#if defined(__AVR__)
#include <avr/pgmspace.h>
#define pgm_read_qword(x) \
  (__extension__({ \
    const uint32_t *_temp = (const uint32_t *) (x); \
    ((uint64_t) pgm_read_dword(_temp)) | (((uint64_t) pgm_read_dword(_temp + 1)) << 32); \
  }))
#elif defined(ESP8266) || defined(ESP32)
#include <pgmspace.h>
#define pgm_read_qword(x) \
  (__extension__({ \
    const uint32_t *_temp = (const uint32_t *) (x); \
    ((uint64_t) pgm_read_dword(_temp)) | (((uint64_t) pgm_read_dword(_temp + 1)) << 32); \
  }))
#else
#include <string>
#define PROGMEM
#ifndef pgm_read_byte
#define pgm_read_byte(x) (*(x))
#endif
#ifndef pgm_read_word
#define pgm_read_word(x) (*(x))
#endif
#ifndef pgm_read_dword
#define pgm_read_dword(x) (*(x))
#endif
#ifndef pgm_read_qword
#define pgm_read_qword(x) (*(x))
#endif
#ifndef memcpy_P
#define memcpy_P(d, s, l) memcpy((d), (s), (l))
#endif
#endif

#endif
