#pragma once

#include "component.h"
#include "helpers.h"

namespace esphome {

inline static uint8_t esp_scale8(uint8_t i, uint8_t scale) { return (uint16_t(i) * (1 + uint16_t(scale))) / 256; }
inline static uint8_t esp_scale(uint8_t i, uint8_t scale, uint8_t max_value = 255) { return (max_value * i / scale); }

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
  enum ColorOrder : uint8_t { COLOR_ORDER_RGB = 0, COLOR_ORDER_BGR = 1, COLOR_ORDER_GRB = 2 };
  enum ColorBitness : uint8_t { COLOR_BITNESS_888 = 0, COLOR_BITNESS_565 = 1, COLOR_BITNESS_332 = 2 };
  inline Color() ALWAYS_INLINE : r(0), g(0), b(0), w(0) {}  // NOLINT
  inline Color(float red, float green, float blue) ALWAYS_INLINE : r(uint8_t(red * 255)),
                                                                   g(uint8_t(green * 255)),
                                                                   b(uint8_t(blue * 255)),
                                                                   w(0) {}
  inline Color(float red, float green, float blue, float white) ALWAYS_INLINE : r(uint8_t(red * 255)),
                                                                                g(uint8_t(green * 255)),
                                                                                b(uint8_t(blue * 255)),
                                                                                w(uint8_t(white * 255)) {}
  inline Color(uint32_t colorcode) ALWAYS_INLINE : r((colorcode >> 16) & 0xFF),
                                                   g((colorcode >> 8) & 0xFF),
                                                   b((colorcode >> 0) & 0xFF),
                                                   w((colorcode >> 24) & 0xFF) {}
  inline Color(uint32_t colorcode, ColorOrder color_order, ColorBitness color_bitness = ColorBitness::COLOR_BITNESS_888,
               bool right_bit_aligned = true) {
    uint8_t first_color, second_color, third_color;
    uint8_t first_bits = 0;
    uint8_t second_bits = 0;
    uint8_t third_bits = 0;

    switch (color_bitness) {
      case COLOR_BITNESS_888:
        first_bits = 8;
        second_bits = 8;
        third_bits = 8;
        break;
      case COLOR_BITNESS_565:
        first_bits = 5;
        second_bits = 6;
        third_bits = 5;
        break;
      case COLOR_BITNESS_332:
        first_bits = 3;
        second_bits = 3;
        third_bits = 2;
        break;
    }

    first_color = right_bit_aligned ? esp_scale(((colorcode >> (second_bits + third_bits)) & ((1 << first_bits) - 1)),
                                                ((1 << first_bits) - 1))
                                    : esp_scale(((colorcode >> 16) & 0xFF), (1 << first_bits) - 1);

    second_color = right_bit_aligned
                       ? esp_scale(((colorcode >> third_bits) & ((1 << second_bits) - 1)), ((1 << second_bits) - 1))
                       : esp_scale(((colorcode >> 8) & 0xFF), ((1 << second_bits) - 1));

    third_color = (right_bit_aligned ? esp_scale(((colorcode >> 0) & 0xFF), ((1 << third_bits) - 1))
                                     : esp_scale(((colorcode >> 0) & 0xFF), (1 << third_bits) - 1));

    switch (color_order) {
      case COLOR_ORDER_RGB:
        this->r = first_color;
        this->g = second_color;
        this->b = third_color;
        break;
      case COLOR_ORDER_BGR:
        this->b = first_color;
        this->g = second_color;
        this->r = third_color;
        break;
      case COLOR_ORDER_GRB:
        this->g = first_color;
        this->r = second_color;
        this->b = third_color;
        break;
    }
  }
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
    return Color(esp_scale8(this->red, scale), esp_scale8(this->green, scale), esp_scale8(this->blue, scale),
                 esp_scale8(this->white, scale));
  }
  inline Color &operator*=(uint8_t scale) ALWAYS_INLINE {
    this->red = esp_scale8(this->red, scale);
    this->green = esp_scale8(this->green, scale);
    this->blue = esp_scale8(this->blue, scale);
    this->white = esp_scale8(this->white, scale);
    return *this;
  }
  inline Color operator*(const Color &scale) const ALWAYS_INLINE {
    return Color(esp_scale8(this->red, scale.red), esp_scale8(this->green, scale.green),
                 esp_scale8(this->blue, scale.blue), esp_scale8(this->white, scale.white));
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
  inline Color operator+(uint8_t add) const ALWAYS_INLINE { return (*this) + Color(add, add, add, add); }
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
    return (*this) - Color(subtract, subtract, subtract, subtract);
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
  uint8_t to_332(ColorOrder color_order = ColorOrder::COLOR_ORDER_RGB) const {
    uint16_t red_color, green_color, blue_color;

    red_color = esp_scale8(this->red, ((1 << 3) - 1));
    green_color = esp_scale8(this->green, ((1 << 3) - 1));
    blue_color = esp_scale8(this->blue, (1 << 2) - 1);

    switch (color_order) {
      case COLOR_ORDER_RGB:
        return red_color << 5 | green_color << 2 | blue_color;
      case COLOR_ORDER_BGR:
        return blue_color << 6 | green_color << 3 | red_color;
      case COLOR_ORDER_GRB:
        return green_color << 5 | red_color << 2 | blue_color;
    }
    return 0;
  }
  uint16_t to_565(ColorOrder color_order = ColorOrder::COLOR_ORDER_RGB) const {
    uint16_t red_color, green_color, blue_color;

    red_color = esp_scale8(this->red, ((1 << 5) - 1));
    green_color = esp_scale8(this->green, ((1 << 6) - 1));
    blue_color = esp_scale8(this->blue, (1 << 5) - 1);

    switch (color_order) {
      case COLOR_ORDER_RGB:
        return red_color << 11 | green_color << 5 | blue_color;
      case COLOR_ORDER_BGR:
        return blue_color << 11 | green_color << 5 | red_color;
      case COLOR_ORDER_GRB:
        return green_color << 10 | red_color << 5 | blue_color;
    }
    return 0;
  }
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
