#pragma once

#include "esphome/core/helpers.h"

namespace esphome {

inline static uint8_t esp_scale8(uint8_t i, uint8_t scale) { return (uint16_t(i) * (1 + uint16_t(scale))) / 256; }

struct ESPColor {
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
  inline ESPColor() ALWAYS_INLINE : r(0), g(0), b(0), w(0) {}  // NOLINT
  inline ESPColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t white) ALWAYS_INLINE : r(red),
                                                                                           g(green),
                                                                                           b(blue),
                                                                                           w(white) {}
  inline ESPColor(uint8_t red, uint8_t green, uint8_t blue) ALWAYS_INLINE : r(red), g(green), b(blue), w(0) {}
  inline ESPColor(uint32_t colorcode) ALWAYS_INLINE : r((colorcode >> 16) & 0xFF),
                                                      g((colorcode >> 8) & 0xFF),
                                                      b((colorcode >> 0) & 0xFF),
                                                      w((colorcode >> 24) & 0xFF) {}
  inline ESPColor(const ESPColor &rhs) ALWAYS_INLINE {
    this->r = rhs.r;
    this->g = rhs.g;
    this->b = rhs.b;
    this->w = rhs.w;
  }
  inline bool is_on() ALWAYS_INLINE { return this->raw_32 != 0; }
  inline ESPColor &operator=(const ESPColor &rhs) ALWAYS_INLINE {
    this->r = rhs.r;
    this->g = rhs.g;
    this->b = rhs.b;
    this->w = rhs.w;
    return *this;
  }
  inline ESPColor &operator=(uint32_t colorcode) ALWAYS_INLINE {
    this->w = (colorcode >> 24) & 0xFF;
    this->r = (colorcode >> 16) & 0xFF;
    this->g = (colorcode >> 8) & 0xFF;
    this->b = (colorcode >> 0) & 0xFF;
    return *this;
  }
  inline uint8_t &operator[](uint8_t x) ALWAYS_INLINE { return this->raw[x]; }
  inline ESPColor operator*(uint8_t scale) const ALWAYS_INLINE {
    return ESPColor(esp_scale8(this->red, scale), esp_scale8(this->green, scale), esp_scale8(this->blue, scale),
                    esp_scale8(this->white, scale));
  }
  inline ESPColor &operator*=(uint8_t scale) ALWAYS_INLINE {
    this->red = esp_scale8(this->red, scale);
    this->green = esp_scale8(this->green, scale);
    this->blue = esp_scale8(this->blue, scale);
    this->white = esp_scale8(this->white, scale);
    return *this;
  }
  inline ESPColor operator*(const ESPColor &scale) const ALWAYS_INLINE {
    return ESPColor(esp_scale8(this->red, scale.red), esp_scale8(this->green, scale.green),
                    esp_scale8(this->blue, scale.blue), esp_scale8(this->white, scale.white));
  }
  inline ESPColor &operator*=(const ESPColor &scale) ALWAYS_INLINE {
    this->red = esp_scale8(this->red, scale.red);
    this->green = esp_scale8(this->green, scale.green);
    this->blue = esp_scale8(this->blue, scale.blue);
    this->white = esp_scale8(this->white, scale.white);
    return *this;
  }
  inline ESPColor operator+(const ESPColor &add) const ALWAYS_INLINE {
    ESPColor ret;
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
  inline ESPColor &operator+=(const ESPColor &add) ALWAYS_INLINE { return *this = (*this) + add; }
  inline ESPColor operator+(uint8_t add) const ALWAYS_INLINE { return (*this) + ESPColor(add, add, add, add); }
  inline ESPColor &operator+=(uint8_t add) ALWAYS_INLINE { return *this = (*this) + add; }
  inline ESPColor operator-(const ESPColor &subtract) const ALWAYS_INLINE {
    ESPColor ret;
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
  inline ESPColor &operator-=(const ESPColor &subtract) ALWAYS_INLINE { return *this = (*this) - subtract; }
  inline ESPColor operator-(uint8_t subtract) const ALWAYS_INLINE {
    return (*this) - ESPColor(subtract, subtract, subtract, subtract);
  }
  inline ESPColor &operator-=(uint8_t subtract) ALWAYS_INLINE { return *this = (*this) - subtract; }
  static ESPColor random_color() {
    uint32_t rand = random_uint32();
    uint8_t w = rand >> 24;
    uint8_t r = rand >> 16;
    uint8_t g = rand >> 8;
    uint8_t b = rand >> 0;
    const uint16_t max_rgb = std::max(r, std::max(g, b));
    return ESPColor(uint8_t((uint16_t(r) * 255U / max_rgb)), uint8_t((uint16_t(g) * 255U / max_rgb)),
                    uint8_t((uint16_t(b) * 255U / max_rgb)), w);
  }
  ESPColor fade_to_white(uint8_t amnt) { return ESPColor(255, 255, 255, 255) - (*this * amnt); }
  ESPColor fade_to_black(uint8_t amnt) { return *this * amnt; }
  ESPColor lighten(uint8_t delta) { return *this + delta; }
  ESPColor darken(uint8_t delta) { return *this - delta; }

  uint32_t to_rgb_565() const {
    uint32_t color565 = (esp_scale8(this->red, 31) << 11) | (esp_scale8(this->green, 63) << 5) | (esp_scale8(this->blue, 31) << 0);
    return color565;
  }
  uint32_t to_bgr_565() const {
    uint32_t color565 = (esp_scale8(this->blue, 31) << 11) | (esp_scale8(this->green, 63) << 5) | (esp_scale8(this->red, 31) << 0);
    return color565;
  }
  // color list from https://www.rapidtables.com/web/color/RGB_Color.html
  static const ESPColor MAROON;
  static const ESPColor DARK_RED;
  static const ESPColor BROWN;
  static const ESPColor FIREBRICK;
  static const ESPColor CRIMSON;
  static const ESPColor RED;
  static const ESPColor TOMATO;
  static const ESPColor CORAL;
  static const ESPColor INDIAN_RED;
  static const ESPColor LIGHT_CORAL;
  static const ESPColor DARK_SALMON;
  static const ESPColor SALMON;
  static const ESPColor LIGHT_SALMON;
  static const ESPColor ORANGE_RED;
  static const ESPColor DARK_ORANGE;
  static const ESPColor ORANGE;
  static const ESPColor GOLD;
  static const ESPColor DARK_GOLDEN_ROD;
  static const ESPColor GOLDEN_ROD;
  static const ESPColor PALE_GOLDEN_ROD;
  static const ESPColor DARK_KHAKI;
  static const ESPColor KHAKI;
  static const ESPColor OLIVE;
  static const ESPColor YELLOW;
  static const ESPColor YELLOW_GREEN;
  static const ESPColor DARK_OLIVE_GREEN;
  static const ESPColor OLIVE_DRAB;
  static const ESPColor LAWN_GREEN;
  static const ESPColor CHART_REUSE;
  static const ESPColor GREEN_YELLOW;
  static const ESPColor DARK_GREEN;
  static const ESPColor GREEN;
  static const ESPColor FOREST_GREEN;
  static const ESPColor LIME;
  static const ESPColor LIME_GREEN;
  static const ESPColor LIGHT_GREEN;
  static const ESPColor PALE_GREEN;
  static const ESPColor DARK_SEA_GREEN;
  static const ESPColor MEDIUM_SPRING_GREEN;
  static const ESPColor SPRING_GREEN;
  static const ESPColor SEA_GREEN;
  static const ESPColor MEDIUM_AQUA_MARINE;
  static const ESPColor MEDIUM_SEA_GREEN;
  static const ESPColor LIGHT_SEA_GREEN;
  static const ESPColor DARK_SLATE_GRAY;
  static const ESPColor TEAL;
  static const ESPColor DARK_CYAN;
  static const ESPColor CYAN;
  static const ESPColor LIGHT_CYAN;
  static const ESPColor DARK_TURQUOISE;
  static const ESPColor TURQUOISE;
  static const ESPColor MEDIUM_TURQUOISE;
  static const ESPColor PALE_TURQUOISE;
  static const ESPColor AQUA_MARINE;
  static const ESPColor POWDER_BLUE;
  static const ESPColor CADET_BLUE;
  static const ESPColor STEEL_BLUE;
  static const ESPColor CORN_FLOWER_BLUE;
  static const ESPColor DEEP_SKY_BLUE;
  static const ESPColor DODGER_BLUE;
  static const ESPColor LIGHT_BLUE;
  static const ESPColor SKY_BLUE;
  static const ESPColor LIGHT_SKY_BLUE;
  static const ESPColor MIDNIGHT_BLUE;
  static const ESPColor NAVY;
  static const ESPColor DARK_BLUE;
  static const ESPColor MEDIUM_BLUE;
  static const ESPColor BLUE;
  static const ESPColor ROYAL_BLUE;
  static const ESPColor BLUE_VIOLET;
  static const ESPColor INDIGO;
  static const ESPColor DARK_SLATE_BLUE;
  static const ESPColor SLATE_BLUE;
  static const ESPColor MEDIUM_SLATE_BLUE;
  static const ESPColor MEDIUM_PURPLE;
  static const ESPColor DARK_MAGENTA;
  static const ESPColor DARK_VIOLET;
  static const ESPColor DARK_ORCHID;
  static const ESPColor MEDIUM_ORCHID;
  static const ESPColor PURPLE;
  static const ESPColor THISTLE;
  static const ESPColor PLUM;
  static const ESPColor VIOLET;
  static const ESPColor MAGENTA;
  static const ESPColor ORCHID;
  static const ESPColor MEDIUM_VIOLET_RED;
  static const ESPColor PALE_VIOLET_RED;
  static const ESPColor DEEP_PINK;
  static const ESPColor HOT_PINK;
  static const ESPColor LIGHT_PINK;
  static const ESPColor PINK;
  static const ESPColor ANTIQUE_WHITE;
  static const ESPColor BEIGE;
  static const ESPColor BISQUE;
  static const ESPColor BLANCHED_ALMOND;
  static const ESPColor WHEAT;
  static const ESPColor CORN_SILK;
  static const ESPColor LEMON_CHIFFON;
  static const ESPColor LIGHT_GOLDEN_ROD_YELLOW;
  static const ESPColor LIGHT_YELLOW;
  static const ESPColor SADDLE_BROWN;
  static const ESPColor SIENNA;
  static const ESPColor CHOCOLATE;
  static const ESPColor PERU;
  static const ESPColor SANDY_BROWN;
  static const ESPColor BURLY_WOOD;
  static const ESPColor TAN;
  static const ESPColor ROSY_BROWN;
  static const ESPColor MOCCASIN;
  static const ESPColor NAVAJO_WHITE;
  static const ESPColor PEACH_PUFF;
  static const ESPColor MISTY_ROSE;
  static const ESPColor LAVENDER_BLUSH;
  static const ESPColor LINEN;
  static const ESPColor OLD_LACE;
  static const ESPColor PAPAYA_WHIP;
  static const ESPColor SEA_SHELL;
  static const ESPColor MINT_CREAM;
  static const ESPColor SLATE_GRAY;
  static const ESPColor LIGHT_SLATE_GRAY;
  static const ESPColor LIGHT_STEEL_BLUE;
  static const ESPColor LAVENDER;
  static const ESPColor FLORAL_WHITE;
  static const ESPColor ALICE_BLUE;
  static const ESPColor GHOST_WHITE;
  static const ESPColor HONEYDEW;
  static const ESPColor IVORY;
  static const ESPColor AZURE;
  static const ESPColor SNOW;
  static const ESPColor BLACK;
  static const ESPColor DIM_GRAY;
  static const ESPColor GRAY;
  static const ESPColor DARK_GRAY;
  static const ESPColor SILVER;
  static const ESPColor LIGHT_GRAY;
  static const ESPColor GAINSBORO;
  static const ESPColor WHITE_SMOKE;
  static const ESPColor WHITE;
};

}  // namespace esphome
