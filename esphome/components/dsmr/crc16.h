/* CRC compatibility, adapted from the Teensy 3 core at:
   https://github.com/PaulStoffregen/cores/tree/master/teensy3
   which was in turn adapted by Paul Stoffregen from the C-only comments here:
   http://svn.savannah.nongnu.org/viewvc/trunk/avr-libc/include/util/crc16.h?revision=933&root=avr-libc&view=markup */

/* Copyright (c) 2002, 2003, 2004  Marek Michalkiewicz
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

   * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.

   * Neither the name of the copyright holders nor the names of
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE. */

#pragma once

#include <stdint.h>

static inline uint16_t _crc16_update(uint16_t crc, uint8_t data) __attribute__((always_inline, unused));
static inline uint16_t _crc16_update(uint16_t crc, uint8_t data)
{
  unsigned int i;

  crc ^= data;
  for (i = 0; i < 8; ++i)
  {
    if (crc & 1)
    {
      crc = (crc >> 1) ^ 0xA001;
    }
    else
    {
      crc = (crc >> 1);
    }
  }
  return crc;
}

static inline uint16_t _crc_xmodem_update(uint16_t crc, uint8_t data) __attribute__((always_inline, unused));
static inline uint16_t _crc_xmodem_update(uint16_t crc, uint8_t data)
{
  unsigned int i;

  crc = crc ^ ((uint16_t)data << 8);
  for (i = 0; i < 8; i++)
  {
    if (crc & 0x8000)
    {
      crc = (crc << 1) ^ 0x1021;
    }
    else
    {
      crc <<= 1;
    }
  }
  return crc;
}

static inline uint16_t _crc_ccitt_update(uint16_t crc, uint8_t data) __attribute__((always_inline, unused));
static inline uint16_t _crc_ccitt_update(uint16_t crc, uint8_t data)
{
  data ^= (crc & 255);
  data ^= data << 4;

  return ((((uint16_t)data << 8) | (crc >> 8)) ^ (uint8_t)(data >> 4) ^ ((uint16_t)data << 3));
}

static inline uint8_t _crc_ibutton_update(uint8_t crc, uint8_t data) __attribute__((always_inline, unused));
static inline uint8_t _crc_ibutton_update(uint8_t crc, uint8_t data)
{
  unsigned int i;

  crc = crc ^ data;
  for (i = 0; i < 8; i++)
  {
    if (crc & 0x01)
    {
      crc = (crc >> 1) ^ 0x8C;
    }
    else
    {
      crc >>= 1;
    }
  }
  return crc;
}
