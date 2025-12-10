#pragma once

#include "defines.h"
#include "component.h"
#include "helpers.h"

#ifdef USE_LVGL
#include "esphome/components/lvgl/lvgl_proxy.h"
#endif  // USE_LVGL

namespace esphome {

inline static constexpr uint8_t esp_scale8(uint8_t i, uint8_t scale) {
  return (uint16_t(i) * (1 + uint16_t(scale))) / 256;
}

/// Scale an 8-bit value by two 8-bit scale factors with improved precision.
/// This is more accurate than calling esp_scale8() twice because it delays
/// truncation until after both multiplications, preserving intermediate precision.
/// For example: esp_scale8_twice(value, max_brightness, local_brightness)
/// gives better results than esp_scale8(esp_scale8(value, max_brightness), local_brightness)
inline static constexpr uint8_t esp_scale8_twice(uint8_t i, uint8_t scale1, uint8_t scale2) {
  return (uint32_t(i) * (1 + uint32_t(scale1)) * (1 + uint32_t(scale2))) >> 16;
}

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
      union {
        uint8_t cw;
        uint8_t cold_white;
      };
      union {
        uint8_t ww;
        uint8_t warm_white;
      };
    };
    uint8_t raw[8];
    uint32_t raw_32;
    uint64_t raw_64;
  };

#ifdef USE_LVGL
  // convenience function for Color to get a lv_color_t representation
  operator lv_color_t() const { return lv_color_make(this->r, this->g, this->b); }
#endif

  inline constexpr Color() ESPHOME_ALWAYS_INLINE : raw_64(0) {}  // NOLINT
  inline constexpr Color(uint8_t red, uint8_t green, uint8_t blue) ESPHOME_ALWAYS_INLINE : r(red),
                                                                                           g(green),
                                                                                           b(blue),
                                                                                           w(0),
                                                                                           cw(0),
                                                                                           ww(0) {}

  inline constexpr Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t white) ESPHOME_ALWAYS_INLINE : r(red),
                                                                                                          g(green),
                                                                                                          b(blue),
                                                                                                          w(white),
                                                                                                          cw(0),
                                                                                                          ww(0) {}
  inline constexpr Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t cold_white,
                         uint8_t warm_white) ESPHOME_ALWAYS_INLINE : r(red),
                                                                     g(green),
                                                                     b(blue),
                                                                     w(0),
                                                                     cw(cold_white),
                                                                     ww(warm_white) {}

  inline constexpr Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t white, uint8_t cold_white,
                         uint8_t warm_white) ESPHOME_ALWAYS_INLINE : r(red),
                                                                     g(green),
                                                                     b(blue),
                                                                     w(white),
                                                                     cw(cold_white),
                                                                     ww(warm_white) {}

  inline explicit constexpr Color(uint32_t colorcode) ESPHOME_ALWAYS_INLINE : r((colorcode >> 16) & 0xFF),
                                                                              g((colorcode >> 8) & 0xFF),
                                                                              b((colorcode >> 0) & 0xFF),
                                                                              w((colorcode >> 24) & 0xFF),
                                                                              cw(0),
                                                                              ww(0) {}

  inline explicit constexpr Color(uint64_t colorcode) ESPHOME_ALWAYS_INLINE : r((colorcode >> 16) & 0xFF),
                                                                              g((colorcode >> 8) & 0xFF),
                                                                              b((colorcode >> 0) & 0xFF),
                                                                              w(0),
                                                                              cw((colorcode >> 24) & 0xFF),
                                                                              ww((colorcode >> 32) & 0xFF) {}

  inline bool is_on() ESPHOME_ALWAYS_INLINE { return this->raw_64 != 0; }

  inline bool operator==(const Color &rhs) {  // NOLINT
    return this->raw_64 == rhs.raw_64;
  }
  inline bool operator==(uint32_t colorcode) {  // NOLINT
    return this->raw_32 == colorcode;
  }
  inline bool operator==(uint64_t colorcode) {  // NOLINT
    return this->raw_64 == colorcode;
  }
  inline bool operator!=(const Color &rhs) {  // NOLINT
    return this->raw_64 != rhs.raw_64;
  }
  inline bool operator!=(uint32_t colorcode) {  // NOLINT
    return this->raw_32 != colorcode;
  }
  inline bool operator!=(uint64_t colorcode) {  // NOLINT
    return this->raw_64 != colorcode;
  }
  inline uint8_t &operator[](uint8_t x) ESPHOME_ALWAYS_INLINE { return this->raw[x]; }
  inline Color operator*(uint8_t scale) const ESPHOME_ALWAYS_INLINE {
    return Color(esp_scale8(this->red, scale), esp_scale8(this->green, scale), esp_scale8(this->blue, scale),
                 esp_scale8(this->white, scale), esp_scale8(this->cold_white, scale),
                 esp_scale8(this->warm_white, scale));
  }
  inline Color operator~() const ESPHOME_ALWAYS_INLINE {
    return Color(255 - this->red, 255 - this->green, 255 - this->blue);
  }
  inline Color &operator*=(uint8_t scale) ESPHOME_ALWAYS_INLINE {
    this->red = esp_scale8(this->red, scale);
    this->green = esp_scale8(this->green, scale);
    this->blue = esp_scale8(this->blue, scale);
    this->white = esp_scale8(this->white, scale);
    this->cold_white = esp_scale8(this->cold_white, scale);
    this->warm_white = esp_scale8(this->warm_white, scale);
    return *this;
  }
  inline Color operator*(const Color &scale) const ESPHOME_ALWAYS_INLINE {
    return Color(esp_scale8(this->red, scale.red), esp_scale8(this->green, scale.green),
                 esp_scale8(this->blue, scale.blue), esp_scale8(this->white, scale.white),
                 esp_scale8(this->cold_white, scale.cold_white), esp_scale8(this->warm_white, scale.warm_white));
  }
  inline Color &operator*=(const Color &scale) ESPHOME_ALWAYS_INLINE {
    this->red = esp_scale8(this->red, scale.red);
    this->green = esp_scale8(this->green, scale.green);
    this->blue = esp_scale8(this->blue, scale.blue);
    this->white = esp_scale8(this->white, scale.white);
    this->cold_white = esp_scale8(this->cold_white, scale.cold_white);
    this->warm_white = esp_scale8(this->warm_white, scale.warm_white);
    return *this;
  }
  inline Color operator+(const Color &add) const ESPHOME_ALWAYS_INLINE {
    Color ret;
    if (uint8_t(add.r + this->r) < this->r) {
      ret.r = 255;
    } else {
      ret.r = this->r + add.r;
    }
    if (uint8_t(add.g + this->g) < this->g) {
      ret.g = 255;
    } else {
      ret.g = this->g + add.g;
    }
    if (uint8_t(add.b + this->b) < this->b) {
      ret.b = 255;
    } else {
      ret.b = this->b + add.b;
    }
    if (uint8_t(add.w + this->w) < this->w) {
      ret.w = 255;
    } else {
      ret.w = this->w + add.w;
    }
    if (uint8_t(add.cw + this->cw) < this->cw) {
      ret.cw = 255;
    } else {
      ret.cw = this->cw + add.cw;
    }
    if (uint8_t(add.ww + this->ww) < this->ww) {
      ret.ww = 255;
    } else {
      ret.ww = this->ww + add.ww;
    }
    return ret;
  }
  inline Color &operator+=(const Color &add) ESPHOME_ALWAYS_INLINE { return *this = (*this) + add; }
  inline Color operator+(uint8_t add) const ESPHOME_ALWAYS_INLINE { return (*this) + Color(add, add, add, add); }
  inline Color &operator+=(uint8_t add) ESPHOME_ALWAYS_INLINE { return *this = (*this) + add; }
  inline Color operator-(const Color &subtract) const ESPHOME_ALWAYS_INLINE {
    Color ret;
    if (subtract.r > this->r) {
      ret.r = 0;
    } else {
      ret.r = this->r - subtract.r;
    }
    if (subtract.g > this->g) {
      ret.g = 0;
    } else {
      ret.g = this->g - subtract.g;
    }
    if (subtract.b > this->b) {
      ret.b = 0;
    } else {
      ret.b = this->b - subtract.b;
    }
    if (subtract.w > this->w) {
      ret.w = 0;
    } else {
      ret.w = this->w - subtract.w;
    }
    if (subtract.cw > this->cw) {
      ret.cw = 0;
    } else {
      ret.cw = this->cw - subtract.cw;
    }
    if (subtract.ww > this->ww) {
      ret.ww = 0;
    } else {
      ret.ww = this->ww - subtract.ww;
    }
    return ret;
  }
  inline Color &operator-=(const Color &subtract) ESPHOME_ALWAYS_INLINE { return *this = (*this) - subtract; }
  inline Color operator-(uint8_t subtract) const ESPHOME_ALWAYS_INLINE {
    return (*this) - Color(subtract, subtract, subtract, subtract);
  }
  inline Color &operator-=(uint8_t subtract) ESPHOME_ALWAYS_INLINE { return *this = (*this) - subtract; }
  static Color random_color() {
    uint64_t rand = (static_cast<uint64_t>(random_uint32()) << 32) + static_cast<uint64_t>(random_uint32());
    uint8_t ww = rand >> 40;
    uint8_t cw = rand >> 32;
    uint8_t w = rand >> 24;
    uint8_t r = rand >> 16;
    uint8_t g = rand >> 8;
    uint8_t b = rand >> 0;
    const uint16_t max_rgb = std::max(r, std::max(g, b));
    return Color(uint8_t((uint16_t(r) * 255U / max_rgb)), uint8_t((uint16_t(g) * 255U / max_rgb)),
                 uint8_t((uint16_t(b) * 255U / max_rgb)), w, cw, ww);
  }

  Color gradient(const Color &to_color, uint8_t amnt);
  Color fade_to_white(uint8_t amnt);
  Color fade_to_black(uint8_t amnt);

  Color lighten(uint8_t delta) { return *this + delta; }
  Color darken(uint8_t delta) { return *this - delta; }

  static const Color BLACK;
  static const Color WHITE;
};

}  // namespace esphome
