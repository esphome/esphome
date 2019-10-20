#ifndef __INC_FASTSPI_TYPES_H
#define __INC_FASTSPI_TYPES_H

#include "FastLED.h"

FASTLED_NAMESPACE_BEGIN

// Some helper macros for getting at mis-ordered byte values
#define SPI_B0 (RGB_BYTE0(RGB_ORDER) + (MASK_SKIP_BITS & SKIP))
#define SPI_B1 (RGB_BYTE1(RGB_ORDER) + (MASK_SKIP_BITS & SKIP))
#define SPI_B2 (RGB_BYTE2(RGB_ORDER) + (MASK_SKIP_BITS & SKIP))
#define SPI_ADVANCE (3 + (MASK_SKIP_BITS & SKIP))

/// Some of the SPI controllers will need to perform a transform on each byte before doing
/// anyting with it.  Creating a class of this form and passing it in as a template parameter to
/// writeBytes/writeBytes3 below will ensure that the body of this method will get called on every
/// byte worked on.  Recommendation, make the adjust method aggressively inlined.
///
/// TODO: Convinience macro for building these
class DATA_NOP {
public:
  static __attribute__((always_inline)) inline uint8_t adjust(register uint8_t data) { return data; }
  static __attribute__((always_inline)) inline uint8_t adjust(register uint8_t data, register uint8_t scale) { return scale8(data, scale); }
  static __attribute__((always_inline)) inline void postBlock(int /* len */) { }
};

#define FLAG_START_BIT 0x80
#define MASK_SKIP_BITS 0x3F

// Clock speed dividers
#define SPEED_DIV_2 2
#define SPEED_DIV_4 4
#define SPEED_DIV_8 8
#define SPEED_DIV_16 16
#define SPEED_DIV_32 32
#define SPEED_DIV_64 64
#define SPEED_DIV_128 128

#define MAX_DATA_RATE 0

FASTLED_NAMESPACE_END

#endif
