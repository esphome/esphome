#ifndef __INC_BITSWAP_H
#define __INC_BITSWAP_H

#include "FastLED.h"

FASTLED_NAMESPACE_BEGIN

///@file bitswap.h
///Functions for rotating bits/bytes

///@defgroup Bitswap Bit swapping/rotate
///Functions for doing a rotation of bits/bytes used by parallel output
///@{
#if defined(FASTLED_ARM) || defined(FASTLED_ESP8266)
/// structure representing 8 bits of access
typedef union {
  uint8_t raw;
  struct {
  uint32_t a0:1;
  uint32_t a1:1;
  uint32_t a2:1;
  uint32_t a3:1;
  uint32_t a4:1;
  uint32_t a5:1;
  uint32_t a6:1;
  uint32_t a7:1;
  };
} just8bits;

/// structure representing 32 bits of access
typedef struct {
  uint32_t a0:1;
  uint32_t a1:1;
  uint32_t a2:1;
  uint32_t a3:1;
  uint32_t a4:1;
  uint32_t a5:1;
  uint32_t a6:1;
  uint32_t a7:1;
  uint32_t b0:1;
  uint32_t b1:1;
  uint32_t b2:1;
  uint32_t b3:1;
  uint32_t b4:1;
  uint32_t b5:1;
  uint32_t b6:1;
  uint32_t b7:1;
  uint32_t c0:1;
  uint32_t c1:1;
  uint32_t c2:1;
  uint32_t c3:1;
  uint32_t c4:1;
  uint32_t c5:1;
  uint32_t c6:1;
  uint32_t c7:1;
  uint32_t d0:1;
  uint32_t d1:1;
  uint32_t d2:1;
  uint32_t d3:1;
  uint32_t d4:1;
  uint32_t d5:1;
  uint32_t d6:1;
  uint32_t d7:1;
} sub4;

/// union containing a full 8 bytes to swap the bit orientation on
typedef union {
  uint32_t word[2];
  uint8_t bytes[8];
  struct {
    sub4 a;
    sub4 b;
  };
} bitswap_type;


#define SWAPSA(X,N) out.  X ## 0 = in.a.a ## N; \
  out.  X ## 1 = in.a.b ## N; \
  out.  X ## 2 = in.a.c ## N; \
  out.  X ## 3 = in.a.d ## N;

#define SWAPSB(X,N) out.  X ## 0 = in.b.a ## N; \
  out.  X ## 1 = in.b.b ## N; \
  out.  X ## 2 = in.b.c ## N; \
  out.  X ## 3 = in.b.d ## N;

#define SWAPS(X,N) out.  X ## 0 = in.a.a ## N; \
  out.  X ## 1 = in.a.b ## N; \
  out.  X ## 2 = in.a.c ## N; \
  out.  X ## 3 = in.a.d ## N; \
  out.  X ## 4 = in.b.a ## N; \
  out.  X ## 5 = in.b.b ## N; \
  out.  X ## 6 = in.b.c ## N; \
  out.  X ## 7 = in.b.d ## N;


/// Do an 8byte by 8bit rotation
__attribute__((always_inline)) inline void swapbits8(bitswap_type in, bitswap_type & out) {

  // SWAPS(a.a,7);
  // SWAPS(a.b,6);
  // SWAPS(a.c,5);
  // SWAPS(a.d,4);
  // SWAPS(b.a,3);
  // SWAPS(b.b,2);
  // SWAPS(b.c,1);
  // SWAPS(b.d,0);

  // SWAPSA(a.a,7);
  // SWAPSA(a.b,6);
  // SWAPSA(a.c,5);
  // SWAPSA(a.d,4);
  //
  // SWAPSB(a.a,7);
  // SWAPSB(a.b,6);
  // SWAPSB(a.c,5);
  // SWAPSB(a.d,4);
  //
  // SWAPSA(b.a,3);
  // SWAPSA(b.b,2);
  // SWAPSA(b.c,1);
  // SWAPSA(b.d,0);
  // //
  // SWAPSB(b.a,3);
  // SWAPSB(b.b,2);
  // SWAPSB(b.c,1);
  // SWAPSB(b.d,0);

  for(int i = 0; i < 8; i++) {
    just8bits work;
    work.a3 = in.word[0] >> 31;
    work.a2 = in.word[0] >> 23;
    work.a1 = in.word[0] >> 15;
    work.a0 = in.word[0] >> 7;
    in.word[0] <<= 1;
    work.a7 = in.word[1] >> 31;
    work.a6 = in.word[1] >> 23;
    work.a5 = in.word[1] >> 15;
    work.a4 = in.word[1] >> 7;
    in.word[1] <<= 1;
    out.bytes[i] = work.raw;
  }
}

/// Slow version of the 8 byte by 8 bit rotation
__attribute__((always_inline)) inline void slowswap(unsigned char *A, unsigned char *B) {

  for(int row = 0; row < 7; row++) {
    uint8_t x = A[row];

    uint8_t bit = (1<<row);
    unsigned char *p = B;
    for(uint32_t mask = 1<<7 ; mask ; mask >>= 1) {
      if(x & mask) {
        *p++ |= bit;
      } else {
        *p++ &= ~bit;
      }
    }
    // B[7] |= (x & 0x01) << row; x >>= 1;
    // B[6] |= (x & 0x01) << row; x >>= 1;
    // B[5] |= (x & 0x01) << row; x >>= 1;
    // B[4] |= (x & 0x01) << row; x >>= 1;
    // B[3] |= (x & 0x01) << row; x >>= 1;
    // B[2] |= (x & 0x01) << row; x >>= 1;
    // B[1] |= (x & 0x01) << row; x >>= 1;
    // B[0] |= (x & 0x01) << row; x >>= 1;
  }
}

void transpose8x1_noinline(unsigned char *A, unsigned char *B);

/// Simplified form of bits rotating function.  Based on code found here - http://www.hackersdelight.org/hdcodetxt/transpose8.c.txt - rotating
/// data into LSB for a faster write (the code using this data can happily walk the array backwards)
__attribute__((always_inline)) inline void transpose8x1(unsigned char *A, unsigned char *B) {
  uint32_t x, y, t;

  // Load the array and pack it into x and y.
  y = *(unsigned int*)(A);
  x = *(unsigned int*)(A+4);

  // pre-transform x
  t = (x ^ (x >> 7)) & 0x00AA00AA;  x = x ^ t ^ (t << 7);
  t = (x ^ (x >>14)) & 0x0000CCCC;  x = x ^ t ^ (t <<14);

  // pre-transform y
  t = (y ^ (y >> 7)) & 0x00AA00AA;  y = y ^ t ^ (t << 7);
  t = (y ^ (y >>14)) & 0x0000CCCC;  y = y ^ t ^ (t <<14);

  // final transform
  t = (x & 0xF0F0F0F0) | ((y >> 4) & 0x0F0F0F0F);
  y = ((x << 4) & 0xF0F0F0F0) | (y & 0x0F0F0F0F);
  x = t;

  *((uint32_t*)B) = y;
  *((uint32_t*)(B+4)) = x;
}

/// Simplified form of bits rotating function.  Based on code  found here - http://www.hackersdelight.org/hdcodetxt/transpose8.c.txt
__attribute__((always_inline)) inline void transpose8x1_MSB(unsigned char *A, unsigned char *B) {
  uint32_t x, y, t;

  // Load the array and pack it into x and y.
  y = *(unsigned int*)(A);
  x = *(unsigned int*)(A+4);

  // pre-transform x
  t = (x ^ (x >> 7)) & 0x00AA00AA;  x = x ^ t ^ (t << 7);
  t = (x ^ (x >>14)) & 0x0000CCCC;  x = x ^ t ^ (t <<14);

  // pre-transform y
  t = (y ^ (y >> 7)) & 0x00AA00AA;  y = y ^ t ^ (t << 7);
  t = (y ^ (y >>14)) & 0x0000CCCC;  y = y ^ t ^ (t <<14);

  // final transform
  t = (x & 0xF0F0F0F0) | ((y >> 4) & 0x0F0F0F0F);
  y = ((x << 4) & 0xF0F0F0F0) | (y & 0x0F0F0F0F);
  x = t;

  B[7] = y; y >>= 8;
  B[6] = y; y >>= 8;
  B[5] = y; y >>= 8;
  B[4] = y;

  B[3] = x; x >>= 8;
  B[2] = x; x >>= 8;
  B[1] = x; x >>= 8;
  B[0] = x; /* */
}

/// templated bit-rotating function.   Based on code found here - http://www.hackersdelight.org/hdcodetxt/transpose8.c.txt
template<int m, int n>
__attribute__((always_inline)) inline void transpose8(unsigned char *A, unsigned char *B) {
  uint32_t x, y, t;

  // Load the array and pack it into x and y.
  if(m == 1) {
    y = *(unsigned int*)(A);
    x = *(unsigned int*)(A+4);
  } else {
    x = (A[0]<<24)   | (A[m]<<16)   | (A[2*m]<<8) | A[3*m];
    y = (A[4*m]<<24) | (A[5*m]<<16) | (A[6*m]<<8) | A[7*m];
  }

  // pre-transform x
  t = (x ^ (x >> 7)) & 0x00AA00AA;  x = x ^ t ^ (t << 7);
  t = (x ^ (x >>14)) & 0x0000CCCC;  x = x ^ t ^ (t <<14);

  // pre-transform y
  t = (y ^ (y >> 7)) & 0x00AA00AA;  y = y ^ t ^ (t << 7);
  t = (y ^ (y >>14)) & 0x0000CCCC;  y = y ^ t ^ (t <<14);

  // final transform
  t = (x & 0xF0F0F0F0) | ((y >> 4) & 0x0F0F0F0F);
  y = ((x << 4) & 0xF0F0F0F0) | (y & 0x0F0F0F0F);
  x = t;

  B[7*n] = y; y >>= 8;
  B[6*n] = y; y >>= 8;
  B[5*n] = y; y >>= 8;
  B[4*n] = y;

  B[3*n] = x; x >>= 8;
  B[2*n] = x; x >>= 8;
  B[n] = x; x >>= 8;
  B[0] = x;
  // B[0]=x>>24;    B[n]=x>>16;    B[2*n]=x>>8;  B[3*n]=x>>0;
  // B[4*n]=y>>24;  B[5*n]=y>>16;  B[6*n]=y>>8;  B[7*n]=y>>0;
}

#endif

FASTLED_NAMESPACE_END

///@}
#endif
