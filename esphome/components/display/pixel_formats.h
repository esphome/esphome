#pragma once

#include <cstdarg>
#include <vector>

#include "esphome/core/helpers.h"

namespace esphome {
namespace display {

enum class PixelFormat {
  Unknown,
  A1,
  W1,
  W2,
  W4,
  W8,
  W8_KEY,
  RGB332,
  RGB565_LE,
  RGB565_BE,
  RGB888,
  RGBA4444,
  RGBA8888
};

#define EXPORT_SRC_PIXEL_FORMAT(MACRO, IGNORE_MACRO, ...) \
  IGNORE_MACRO(Unknown, ##__VA_ARGS__); \
  MACRO(A1, ##__VA_ARGS__); \
  MACRO(W1, ##__VA_ARGS__); \
  MACRO(W2, ##__VA_ARGS__); \
  MACRO(W4, ##__VA_ARGS__); \
  MACRO(W8, ##__VA_ARGS__); \
  MACRO(W8_KEY, ##__VA_ARGS__); \
  MACRO(RGB332, ##__VA_ARGS__); \
  MACRO(RGB565_LE, ##__VA_ARGS__); \
  MACRO(RGB565_BE, ##__VA_ARGS__); \
  MACRO(RGB888, ##__VA_ARGS__); \
  MACRO(RGBA4444, ##__VA_ARGS__); \
  MACRO(RGBA8888, ##__VA_ARGS__);

#define EXPORT_DEST_PIXEL_FORMAT(MACRO, IGNORE_MACRO, ...) \
  IGNORE_MACRO(Unknown, ##__VA_ARGS__); \
  IGNORE_MACRO(A1, ##__VA_ARGS__); \
  MACRO(W1, ##__VA_ARGS__); \
  MACRO(W2, ##__VA_ARGS__); \
  MACRO(W4, ##__VA_ARGS__); \
  MACRO(W8, ##__VA_ARGS__); \
  IGNORE_MACRO(W8_KEY, ##__VA_ARGS__); \
  MACRO(RGB332, ##__VA_ARGS__); \
  MACRO(RGB565_LE, ##__VA_ARGS__); \
  MACRO(RGB565_BE, ##__VA_ARGS__); \
  MACRO(RGB888, ##__VA_ARGS__); \
  MACRO(RGBA4444, ##__VA_ARGS__); \
  MACRO(RGBA8888, ##__VA_ARGS__);

template<PixelFormat FF, int RR, int GG, int BB, int AA, int WW, int NN, int PP = 1, bool CCOLOR_KEY = 0>
struct PixelDetails {
  static const PixelFormat FORMAT = FF;
  static const int R = RR;
  static const int G = GG;
  static const int B = BB;
  static const int A = AA;
  static const int W = WW;
  static const int BYTES = NN;
  static const int PACKED_PIXELS = PP;
  static const bool COLOR_KEY = CCOLOR_KEY;

  static inline ALWAYS_INLINE int packed_pixel(int x) {
    return x % PACKED_PIXELS;
  }
  static inline ALWAYS_INLINE int array_offset(int x) {
    return x / PACKED_PIXELS;
  }
  static inline ALWAYS_INLINE int array_stride(int width) {
    return (width + PACKED_PIXELS - 1) / PACKED_PIXELS;
  }
  static inline ALWAYS_INLINE int array_stride(int width, int height) {
    return array_stride(width) * height;
  }
  static inline ALWAYS_INLINE int bytes_stride(int width) {
    return array_stride(width) * BYTES;
  }
  static inline ALWAYS_INLINE int bytes_stride(int width, int height) {
    return array_stride(width, height) * BYTES;
  }
};

struct PixelRGB332 : PixelDetails<PixelFormat::RGB332, 3,3,2,0,0,1> {
  uint8_t raw_8;

  inline ALWAYS_INLINE bool is_on(int pixel = 0) const {
    return true;
  }
  inline ALWAYS_INLINE bool is_transparent(int pixel = 0) const {
    return false;
  }
  inline ALWAYS_INLINE void encode(uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t w, int pixel = 0) {
    this->raw_8 = r << (3+2) | g << (2) | b;
  }
  inline ALWAYS_INLINE void decode(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a, uint8_t &w, int pixel = 0) const {
    r = this->raw_8 >> (3+2);
    g = (this->raw_8 >> (2)) & ((1<<3) - 1);
    b = (this->raw_8 >> 0) & ((1<<2) - 1);
    w = 0; a = 0;
  }
} PACKED;

template<PixelFormat Format, bool BigEndian>
struct PixelRGB565_Endiness : PixelDetails<Format,5,6,5,0,0,2> {
  uint16_t raw_16;

  inline ALWAYS_INLINE bool is_on(int pixel = 0) const {
    return true;
  }
  inline ALWAYS_INLINE bool is_transparent(int pixel = 0) const {
    return false;
  }
  inline ALWAYS_INLINE void encode(uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t w, int pixel = 0) {
    uint16_t value = r << (6+5) | g << (5) | b;
    this->raw_16 = BigEndian ? convert_big_endian(value) : convert_little_endian(value);
  }
  inline ALWAYS_INLINE void decode(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a, uint8_t &w, int pixel = 0) const {
    uint16_t value = BigEndian ? convert_big_endian(this->raw_16) : convert_little_endian(this->raw_16);
    r = value >> (6+5);
    g = (value >> (5)) & ((1<<6) - 1);
    b = (value >> 0) & ((1<<5) - 1);
    w = 0; a = 0;
  }
} PACKED;

typedef PixelRGB565_Endiness<PixelFormat::RGB565_LE, false> PixelRGB565_LE;
typedef PixelRGB565_Endiness<PixelFormat::RGB565_BE, true> PixelRGB565_BE;

struct PixelRGB888 : PixelDetails<PixelFormat::RGB888,8,8,8,0,0,3> {
  uint8_t raw_8[3];

  inline ALWAYS_INLINE bool is_on(int pixel = 0) const {
    return true;
  }
  inline ALWAYS_INLINE bool is_transparent(int pixel = 0) const {
    return false;
  }
  inline ALWAYS_INLINE void encode(uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t w, int pixel = 0) {
    this->raw_8[0] = r;
    this->raw_8[1] = g;
    this->raw_8[2] = b;
  }
  inline ALWAYS_INLINE void decode(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a, uint8_t &w, int pixel = 0) const {
    r = this->raw_8[0];
    g = this->raw_8[1];
    b = this->raw_8[2];
    a = w = 0;
  }
} PACKED;

struct PixelRGBA4444 : PixelDetails<PixelFormat::RGBA4444,4,4,4,4,0,2> {
  uint8_t raw_8[2];

  inline ALWAYS_INLINE bool is_on(int pixel = 0) const {
    return true;
  }
  inline ALWAYS_INLINE bool is_transparent(int pixel = 0) const {
    return (this->raw_8[1] & 0xF) < 0x8;
  }
  inline ALWAYS_INLINE void encode(uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t w, int pixel = 0) {
    this->raw_8[0] = r << 4 | g;
    this->raw_8[1] = b << 4 | a;
  }
  inline ALWAYS_INLINE void decode(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a, uint8_t &w, int pixel = 0) const {
    r = this->raw_8[0] >> 4;
    g = this->raw_8[0] & 0xF;
    b = this->raw_8[1] >> 4;
    a = this->raw_8[1] & 0xF;
    w = 0;
  }
} PACKED;

struct PixelRGBA8888 : PixelDetails<PixelFormat::RGBA8888,8,8,8,8,0,4> {
  uint8_t raw_8[4];

  inline ALWAYS_INLINE bool is_on(int pixel = 0) const {
    return true;
  }
  inline ALWAYS_INLINE bool is_transparent(int pixel = 0) const {
    return this->raw_8[3] < 0x80;
  }
  inline ALWAYS_INLINE void encode(uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t w, int pixel = 0) {
    this->raw_8[0] = r;
    this->raw_8[1] = g;
    this->raw_8[2] = b;
    this->raw_8[3] = a;
  }
  inline ALWAYS_INLINE void decode(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a, uint8_t &w, int pixel = 0) const {
    r = this->raw_8[0];
    g = this->raw_8[1];
    b = this->raw_8[2];
    a = this->raw_8[3];
    w = 0;
  }
} PACKED;

struct PixelW1 : PixelDetails<PixelFormat::W1,0,0,0,0,1,1,8,true> {
  uint8_t raw_8;

  inline ALWAYS_INLINE bool is_on(int pixel = 0) const {
    return (this->raw_8 & (1<<(7-pixel))) ? true : false;
  }
  inline ALWAYS_INLINE bool is_transparent(int pixel = 0) const {
    return false;
  }
  inline ALWAYS_INLINE void encode(uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t w, int pixel = 0) {
    const int mask = 1<<(7-pixel);
    this->raw_8 &= ~mask;
    if (w)
      this->raw_8 |= mask;
  }
  inline ALWAYS_INLINE void decode(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a, uint8_t &w, int pixel = 0) const {
    r = 0; g = 0; b = 0; a = 0;
    w = (this->raw_8 & (1<<(7-pixel))) ? 1 : 0;
  }
} PACKED;

struct PixelA1 : PixelDetails<PixelFormat::A1,0,0,0,1,0,1,8,true> {
  uint8_t raw_8;

  inline ALWAYS_INLINE bool is_on(int pixel = 0) const {
    return (this->raw_8 & (1<<(7-pixel))) ? true : false;
  }
  inline ALWAYS_INLINE bool is_transparent(int pixel = 0) const {
    return (this->raw_8 & (1<<(7-pixel))) ? false : true;
  }
  inline ALWAYS_INLINE void encode(uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t w, int pixel = 0) {
    const int mask = 1<<(7-pixel);
    this->raw_8 &= ~mask;
    if (a)
      this->raw_8 |= mask;
  }
  inline ALWAYS_INLINE void decode(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a, uint8_t &w, int pixel = 0) const {
    r = 0; g = 0; b = 0; w = 0;
    a = (this->raw_8 & (1<<(7-pixel))) ? 1 : 0;
  }
} PACKED;

struct PixelW2 : PixelDetails<PixelFormat::W2,0,0,0,0,2,1,4> {
  uint8_t raw_8;

  inline ALWAYS_INLINE bool is_on(int pixel = 0) const {
    return true;
  }
  inline ALWAYS_INLINE bool is_transparent(int pixel = 0) const {
    return false;
  }
  inline ALWAYS_INLINE void encode(uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t w, int pixel = 0) {
    this->raw_8 &= ~(3 << (pixel*2));
    this->raw_8 |= w << (pixel*2);
  }
  inline ALWAYS_INLINE void decode(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a, uint8_t &w, int pixel = 0) const {
    r = 0; g = 0; b = 0; a = 0;
    w = (this->raw_8 >> (pixel*2)) & 0x3;
  }
} PACKED;

struct PixelW4 : PixelDetails<PixelFormat::W4,0,0,0,0,4,1,2> {
  uint8_t raw_8;

  inline ALWAYS_INLINE bool is_on(int pixel = 0) const {
    return true;
  }
  inline ALWAYS_INLINE bool is_transparent(int pixel = 0) const {
    return false;
  }
  inline ALWAYS_INLINE void encode(uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t w, int pixel = 0) {
    if (pixel) {
      this->raw_8 &= ~0xF0;
      this->raw_8 |= w << 4;
    } else {
      this->raw_8 &= ~0x0F;
      this->raw_8 |= w;
    }
  }
  inline ALWAYS_INLINE void decode(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a, uint8_t &w, int pixel = 0) const {
    r = 0; g = 0; b = 0; a = 0;
    if (pixel)
      w = this->raw_8 >> 4;
    else
      w = this->raw_8 & 0xF;
  }
} PACKED;

template<PixelFormat Format, bool Key>
struct PixelW8_Keyed : PixelDetails<Format,0,0,0,0,8,1> {
  uint8_t raw_8;

  inline ALWAYS_INLINE bool is_on(int pixel = 0) const {
    return true;
  }
  inline ALWAYS_INLINE bool is_transparent(int pixel = 0) const {
    return Key ? this->raw_8 == 1 : false;
  }
  inline ALWAYS_INLINE void encode(uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t w, int pixel = 0) {
    this->raw_8 = w;
  }
  inline ALWAYS_INLINE void decode(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a, uint8_t &w, int pixel = 0) const {
    r = 0; g = 0; b = 0; a = 0;
    w = this->raw_8;
  }
} PACKED;

typedef PixelW8_Keyed<PixelFormat::W8, false> PixelW8;
typedef PixelW8_Keyed<PixelFormat::W8_KEY, true> PixelW8_KEY;

} // display
} // esphome
