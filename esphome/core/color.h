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
  inline Color(uint8_t red, uint8_t green, uint8_t blue) ALWAYS_INLINE : r(red), g(green), b(blue), w(0) {}

  inline Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t white) ALWAYS_INLINE : r(red),
                                                                                        g(green),
                                                                                        b(blue),
                                                                                        w(white) {}
  inline explicit Color(uint32_t colorcode) ALWAYS_INLINE : r((colorcode >> 16) & 0xFF),
                                                            g((colorcode >> 8) & 0xFF),
                                                            b((colorcode >> 0) & 0xFF),
                                                            w((colorcode >> 24) & 0xFF) {}

  inline bool is_on() ALWAYS_INLINE { return this->raw_32 != 0; }

  inline bool operator==(const Color &rhs) {  // NOLINT
    return this->raw_32 == rhs.raw_32;
  }
  inline bool operator==(uint32_t colorcode) {  // NOLINT
    return this->raw_32 == colorcode;
  }
  inline bool operator!=(const Color &rhs) {  // NOLINT
    return this->raw_32 != rhs.raw_32;
  }
  inline bool operator!=(uint32_t colorcode) {  // NOLINT
    return this->raw_32 != colorcode;
  }

  inline Color &operator=(const Color &rhs) ALWAYS_INLINE {  // NOLINT
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
    uint32_t rand = random_uint32();
    uint8_t w = rand >> 24;
    uint8_t r = rand >> 16;
    uint8_t g = rand >> 8;
    uint8_t b = rand >> 0;
    const uint16_t max_rgb = std::max(r, std::max(g, b));
    return Color(uint8_t((uint16_t(r) * 255U / max_rgb)), uint8_t((uint16_t(g) * 255U / max_rgb)),
                 uint8_t((uint16_t(b) * 255U / max_rgb)), w);
  }

  Color gradient(const Color &to_color, uint8_t amnt) {
    Color new_color;
    float amnt_f = float(amnt) / 255.0f;
    new_color.r = amnt_f * (to_color.r - (*this).r) + (*this).r;
    new_color.g = amnt_f * (to_color.g - (*this).g) + (*this).g;
    new_color.b = amnt_f * (to_color.b - (*this).b) + (*this).b;
    new_color.w = amnt_f * (to_color.w - (*this).w) + (*this).w;
    return new_color;
  }
  Color fade_to_white(uint8_t amnt) { return (*this).gradient(Color::WHITE, amnt); }
  Color fade_to_black(uint8_t amnt) { return (*this).gradient(Color::BLACK, amnt); }

  Color lighten(uint8_t delta) { return *this + delta; }
  Color darken(uint8_t delta) { return *this - delta; }

  static const Color BLACK;
  static const Color WHITE;
};

ESPDEPRECATED("Use Color::BLACK instead of COLOR_BLACK", "v1.21")
extern const Color COLOR_BLACK;
ESPDEPRECATED("Use Color::WHITE instead of COLOR_WHITE", "v1.21")
extern const Color COLOR_WHITE;

}  // namespace esphome
