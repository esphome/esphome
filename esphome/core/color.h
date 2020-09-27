#pragma once

#include "component.h"
#include "helpers.h"

namespace esphome {

inline static uint8_t esp_scale8(uint8_t i, uint8_t scale) { return (uint16_t(i) * (1 + uint16_t(scale))) / 256; }

struct Color {
  union {
    struct {
      union {
        uint8_t r;
        uint8_t red;
      };
      union {
        uint8_t g;
        uint8_t green;
      };
      union {
        uint8_t b;
        uint8_t blue;
      };
      union {
        uint8_t w;
        uint8_t white;
      };
    };
    uint8_t raw[4];
    uint32_t raw_32;
  };
  inline Color() ALWAYS_INLINE : r(0), g(0), b(0), w(0) {}  // NOLINT
  inline Color(float red, float green, float blue, float white = 0.f) ALWAYS_INLINE : r(uint8_t(red * 255)),
                                                                                      g(uint8_t(green * 255)),
                                                                                      b(uint8_t(blue * 255)),
                                                                                      w(uint8_t(white * 255)) {}
  inline explicit Color(uint32_t colorcode) ALWAYS_INLINE : r((colorcode >> 16) & 0xFF),
                                                            g((colorcode >> 8) & 0xFF),
                                                            b((colorcode >> 0) & 0xFF),
                                                            w((colorcode >> 24) & 0xFF) {}
  inline bool is_on() ALWAYS_INLINE { return this->raw_32 != 0; }
  inline Color &operator=(const Color &rhs) ALWAYS_INLINE {
    this->r = rhs.r;
    this->g = rhs.g;
    this->b = rhs.b;
    this->w = rhs.w;
    return *this;
  }
  inline Color &operator=(uint32_t colorcode) ALWAYS_INLINE {
    this->w = (colorcode >> 24) & 0xFF;
    this->r = (colorcode >> 16) & 0xFF;
    this->g = (colorcode >> 8) & 0xFF;
    this->b = (colorcode >> 0) & 0xFF;
    return *this;
  }
  inline uint8_t &operator[](uint8_t x) ALWAYS_INLINE { return this->raw[x]; }
  inline Color operator*(uint8_t scale) const ALWAYS_INLINE {
    Color out;
    out.red = esp_scale8(this->red, scale);
    out.green = esp_scale8(this->green, scale);
    out.blue = esp_scale8(this->blue, scale);
    out.white = esp_scale8(this->white, scale);
    return out;
  }
  inline Color &operator*=(uint8_t scale) ALWAYS_INLINE {
    this->red = esp_scale8(this->red, scale);
    this->green = esp_scale8(this->green, scale);
    this->blue = esp_scale8(this->blue, scale);
    this->white = esp_scale8(this->white, scale);
    return *this;
  }
  inline Color operator*(const Color &scale) const ALWAYS_INLINE {
    Color out;
    out.red = esp_scale8(this->red, scale.red);
    out.green = esp_scale8(this->green, scale.green);
    out.blue = esp_scale8(this->blue, scale.blue);
    out.white = esp_scale8(this->white, scale.white);
    return out;
  }
  inline Color &operator*=(const Color &scale) ALWAYS_INLINE {
    this->red = esp_scale8(this->red, scale.red);
    this->green = esp_scale8(this->green, scale.green);
    this->blue = esp_scale8(this->blue, scale.blue);
    this->white = esp_scale8(this->white, scale.white);
    return *this;
  }
  inline Color operator+(const Color &add) const ALWAYS_INLINE {
    Color ret;
    if (uint8_t(add.r + this->r) < this->r)
      ret.r = 255;
    else
      ret.r = this->r + add.r;
    if (uint8_t(add.g + this->g) < this->g)
      ret.g = 255;
    else
      ret.g = this->g + add.g;
    if (uint8_t(add.b + this->b) < this->b)
      ret.b = 255;
    else
      ret.b = this->b + add.b;
    if (uint8_t(add.w + this->w) < this->w)
      ret.w = 255;
    else
      ret.w = this->w + add.w;
    return ret;
  }
  inline Color &operator+=(const Color &add) ALWAYS_INLINE { return *this = (*this) + add; }
  inline Color operator+(uint8_t add) const ALWAYS_INLINE {
    Color addend;
    addend.r = addend.g = addend.b = addend.w = add;
    return (*this) + addend;
  }
  inline Color &operator+=(uint8_t add) ALWAYS_INLINE { return *this = (*this) + add; }
  inline Color operator-(const Color &subtract) const ALWAYS_INLINE {
    Color ret;
    if (subtract.r > this->r)
      ret.r = 0;
    else
      ret.r = this->r - subtract.r;
    if (subtract.g > this->g)
      ret.g = 0;
    else
      ret.g = this->g - subtract.g;
    if (subtract.b > this->b)
      ret.b = 0;
    else
      ret.b = this->b - subtract.b;
    if (subtract.w > this->w)
      ret.w = 0;
    else
      ret.w = this->w - subtract.w;
    return ret;
  }
  inline Color &operator-=(const Color &subtract) ALWAYS_INLINE { return *this = (*this) - subtract; }
  inline Color operator-(uint8_t subtract) const ALWAYS_INLINE {
    Color subtrahend;
    subtrahend.r = subtrahend.g = subtrahend.b = subtrahend.w = subtract;
    return (*this) - subtrahend;
  }
  inline Color &operator-=(uint8_t subtract) ALWAYS_INLINE { return *this = (*this) - subtract; }
  static Color random_color() {
    float r = float(random_uint32()) / float(UINT32_MAX);
    float g = float(random_uint32()) / float(UINT32_MAX);
    float b = float(random_uint32()) / float(UINT32_MAX);
    float w = float(random_uint32()) / float(UINT32_MAX);
    return Color(r, g, b, w);
  }
  Color fade_to_white(uint8_t amnt) { return Color(1, 1, 1, 1) - (*this * amnt); }
  Color fade_to_black(uint8_t amnt) { return *this * amnt; }
  Color lighten(uint8_t delta) { return *this + delta; }
  Color darken(uint8_t delta) { return *this - delta; }

  uint32_t to_rgb_565() const {
    uint32_t color565 =
        (esp_scale8(this->red, 31) << 11) | (esp_scale8(this->green, 63) << 5) | (esp_scale8(this->blue, 31) << 0);
    return color565;
  }
  uint32_t to_bgr_565() const {
    uint32_t color565 =
        (esp_scale8(this->blue, 31) << 11) | (esp_scale8(this->green, 63) << 5) | (esp_scale8(this->red, 31) << 0);
    return color565;
  }
  uint32_t to_grayscale4() const {
    uint32_t gs4 = esp_scale8(this->white, 15);
    return gs4;
  }
};
static const Color COLOR_BLACK(0, 0, 0);
static const Color COLOR_WHITE(1, 1, 1);
};  // namespace esphome
