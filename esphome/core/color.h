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

// static const Color COLOR_MAROON(0.502f, 0, 0);
// static const Color COLOR_DARK_RED(0.545, 0, 0);
// static const Color COLOR_BROWN(0.647, 0.165, 0.165);
// static const Color COLOR_FIREBRICK(0.698, 0.133, 0.133);
// static const Color COLOR_CRIMSON(0.863, 0.078, 0.235);
// static const Color COLOR_RED(1, 0, 0);
// static const Color COLOR_TOMATO(1, 0.388, 0.278);
// static const Color COLOR_CORAL(1, 0.498, 0.314);
// static const Color COLOR_INDIAN_RED(0.804, 0.361, 0.361);
// static const Color COLOR_LIGHT_CORAL(0.941, 0.502, 0.502);
// static const Color COLOR_DARK_SALMON(0.914, 0.588, 0.478);
// static const Color COLOR_SALMON(0.98, 0.502, 0.447);
// static const Color COLOR_LIGHT_SALMON(1, 0.627, 0.478);
// static const Color COLOR_ORANGE_RED(1, 0.271, 0);
// static const Color COLOR_DARK_ORANGE(1, 0.549, 0);
// static const Color COLOR_ORANGE(1, 0.647, 0);
// static const Color COLOR_GOLD(1, 0.843, 0);
// static const Color COLOR_DARK_GOLDEN_ROD(0.722, 0.525, 0.043);
// static const Color COLOR_GOLDEN_ROD(0.855, 0.647, 0.125);
// static const Color COLOR_PALE_GOLDEN_ROD(0.933, 0.91, 0.667);
// static const Color COLOR_DARK_KHAKI(0.741, 0.718, 0.42);
// static const Color COLOR_KHAKI(0.941, 0.902, 0.549);
// static const Color COLOR_OLIVE(0.502, 0.502, 0);
// static const Color COLOR_YELLOW(1, 1, 0);
// static const Color COLOR_YELLOW_GREEN(0.604, 0.804, 0.196);
// static const Color COLOR_DARK_OLIVE_GREEN(0.333, 0.42, 0.184);
// static const Color COLOR_OLIVE_DRAB(0.42, 0.557, 0.137);
// static const Color COLOR_LAWN_GREEN(0.486, 0.988, 0);
// static const Color COLOR_CHART_REUSE(0.498, 1, 0);
// static const Color COLOR_GREEN_YELLOW(0.678, 1, 0.184);
// static const Color COLOR_DARK_GREEN(0, 0.392, 0);
static const Color COLOR_GREEN(0, 0.502, 0);
// static const Color COLOR_FOREST_GREEN(0.133, 0.545, 0.133);
// static const Color COLOR_LIME(0, 1, 0);
// static const Color COLOR_LIME_GREEN(0.196, 0.804, 0.196);
// static const Color COLOR_LIGHT_GREEN(0.565, 0.933, 0.565);
// static const Color COLOR_PALE_GREEN(0.596, 0.984, 0.596);
// static const Color COLOR_DARK_SEA_GREEN(0.561, 0.737, 0.561);
// static const Color COLOR_MEDIUM_SPRING_GREEN(0, 0.98, 0.604);
// static const Color COLOR_SPRING_GREEN(0, 1, 0.498);
// static const Color COLOR_SEA_GREEN(0.18, 0.545, 0.341);
// static const Color COLOR_MEDIUM_AQUA_MARINE(0.4, 0.804, 0.667);
// static const Color COLOR_MEDIUM_SEA_GREEN(0.235, 0.702, 0.443);
// static const Color COLOR_LIGHT_SEA_GREEN(0.125, 0.698, 0.667);
// static const Color COLOR_DARK_SLATE_GRAY(0.184, 0.31, 0.31);
// static const Color COLOR_TEAL(0, 0.502, 0.502);
// static const Color COLOR_DARK_CYAN(0, 0.545, 0.545);
// static const Color COLOR_CYAN(0, 1, 1);
// static const Color COLOR_LIGHT_CYAN(0.878, 1, 1);
// static const Color COLOR_DARK_TURQUOISE(0, 0.808, 0.82);
// static const Color COLOR_TURQUOISE(0.251, 0.878, 0.816);
// static const Color COLOR_MEDIUM_TURQUOISE(0.282, 0.82, 0.8);
// static const Color COLOR_PALE_TURQUOISE(0.686, 0.933, 0.933);
// static const Color COLOR_AQUA_MARINE(0.498, 1, 0.831);
// static const Color COLOR_POWDER_BLUE(0.69, 0.878, 0.902);
// static const Color COLOR_CADET_BLUE(0.373, 0.62, 0.627);
// static const Color COLOR_STEEL_BLUE(0.275, 0.51, 0.706);
// static const Color COLOR_CORN_FLOWER_BLUE(0.392, 0.584, 0.929);
// static const Color COLOR_DEEP_SKY_BLUE(0, 0.749, 1);
// static const Color COLOR_DODGER_BLUE(0.118, 0.565, 1);
// static const Color COLOR_LIGHT_BLUE(0.678, 0.847, 0.902);
// static const Color COLOR_SKY_BLUE(0.529, 0.808, 0.922);
// static const Color COLOR_LIGHT_SKY_BLUE(0.529, 0.808, 0.98);
// static const Color COLOR_MIDNIGHT_BLUE(0.098, 0.098, 0.439);
// static const Color COLOR_NAVY(0, 0, 0.502);
// static const Color COLOR_DARK_BLUE(0, 0, 0.545);
// static const Color COLOR_MEDIUM_BLUE(0, 0, 0.804);
// static const Color COLOR_BLUE(0, 0, 1);
// static const Color COLOR_ROYAL_BLUE(0.255, 0.412, 0.882);
// static const Color COLOR_BLUE_VIOLET(0.541, 0.169, 0.886);
// static const Color COLOR_INDIGO(0.294, 0, 0.51);
// static const Color COLOR_DARK_SLATE_BLUE(0.282, 0.239, 0.545);
// static const Color COLOR_SLATE_BLUE(0.416, 0.353, 0.804);
// static const Color COLOR_MEDIUM_SLATE_BLUE(0.482, 0.408, 0.933);
// static const Color COLOR_MEDIUM_PURPLE(0.576, 0.439, 0.859);
// static const Color COLOR_DARK_MAGENTA(0.545, 0, 0.545);
// static const Color COLOR_DARK_VIOLET(0.58, 0, 0.827);
// static const Color COLOR_DARK_ORCHID(0.6, 0.196, 0.8);
// static const Color COLOR_MEDIUM_ORCHID(0.729, 0.333, 0.827);
// static const Color COLOR_PURPLE(0.502, 0, 0.502);
// static const Color COLOR_THISTLE(0.847, 0.749, 0.847);
// static const Color COLOR_PLUM(0.867, 0.627, 0.867);
// static const Color COLOR_VIOLET(0.933, 0.51, 0.933);
// static const Color COLOR_MAGENTA(1, 0, 1);
// static const Color COLOR_ORCHID(0.855, 0.439, 0.839);
// static const Color COLOR_MEDIUM_VIOLET_RED(0.78, 0.082, 0.522);
// static const Color COLOR_PALE_VIOLET_RED(0.859, 0.439, 0.576);
// static const Color COLOR_DEEP_PINK(1, 0.078, 0.576);
// static const Color COLOR_HOT_PINK(1, 0.412, 0.706);
// static const Color COLOR_LIGHT_PINK(1, 0.714, 0.757);
// static const Color COLOR_PINK(1, 0.753, 0.796);
// static const Color COLOR_ANTIQUE_WHITE(0.98, 0.922, 0.843);
// static const Color COLOR_BEIGE(0.961, 0.961, 0.863);
// static const Color COLOR_BISQUE(1, 0.894, 0.769);
// static const Color COLOR_BLANCHED_ALMOND(1, 0.922, 0.804);
// static const Color COLOR_WHEAT(0.961, 0.871, 0.702);
// static const Color COLOR_CORN_SILK(1, 0.973, 0.863);
// static const Color COLOR_LEMON_CHIFFON(1, 0.98, 0.804);
// static const Color COLOR_LIGHT_GOLDEN_ROD_YELLOW(0.98, 0.98, 0.824);
// static const Color COLOR_LIGHT_YELLOW(1, 1, 0.878);
// static const Color COLOR_SADDLE_BROWN(0.545, 0.271, 0.075);
// static const Color COLOR_SIENNA(0.627, 0.322, 0.176);
// static const Color COLOR_CHOCOLATE(0.824, 0.412, 0.118);
// static const Color COLOR_PERU(0.804, 0.522, 0.247);
// static const Color COLOR_SANDY_BROWN(0.957, 0.643, 0.376);
// static const Color COLOR_BURLY_WOOD(0.871, 0.722, 0.529);
// static const Color COLOR_TAN(0.824, 0.706, 0.549);
// static const Color COLOR_ROSY_BROWN(0.737, 0.561, 0.561);
// static const Color COLOR_MOCCASIN(1, 0.894, 0.71);
// static const Color COLOR_NAVAJO_WHITE(1, 0.871, 0.678);
// static const Color COLOR_PEACH_PUFF(1, 0.855, 0.725);
// static const Color COLOR_MISTY_ROSE(1, 0.894, 0.882);
// static const Color COLOR_LAVENDER_BLUSH(1, 0.941, 0.961);
// static const Color COLOR_LINEN(0.98, 0.941, 0.902);
// static const Color COLOR_OLD_LACE(0.992, 0.961, 0.902);
// static const Color COLOR_PAPAYA_WHIP(1, 0.937, 0.835);
// static const Color COLOR_SEA_SHELL(1, 0.961, 0.933);
// static const Color COLOR_MINT_CREAM(0.961, 1, 0.98);
// static const Color COLOR_SLATE_GRAY(0.439, 0.502, 0.565);
// static const Color COLOR_LIGHT_SLATE_GRAY(0.467, 0.533, 0.6);
// static const Color COLOR_LIGHT_STEEL_BLUE(0.69, 0.769, 0.871);
// static const Color COLOR_LAVENDER(0.902, 0.902, 0.98);
// static const Color COLOR_FLORAL_WHITE(1, 0.98, 0.941);
// static const Color COLOR_ALICE_BLUE(0.941, 0.973, 1);
// static const Color COLOR_GHOST_WHITE(0.973, 0.973, 1);
// static const Color COLOR_HONEYDEW(0.941, 1, 0.941);
// static const Color COLOR_IVORY(1, 1, 0.941);
// static const Color COLOR_AZURE(0.941, 1, 1);
// static const Color COLOR_SNOW(1, 0.98, 0.98);
static const Color COLOR_BLACK(0, 0, 0);
// static const Color COLOR_DIM_GRAY(0.412, 0.412, 0.412);
// static const Color COLOR_GRAY(0.502, 0.502, 0.502);
// static const Color COLOR_DARK_GRAY(0.663, 0.663, 0.663);
// static const Color COLOR_SILVER(0.753, 0.753, 0.753);
// static const Color COLOR_LIGHT_GRAY(0.827, 0.827, 0.827);
// static const Color COLOR_GAINSBORO(0.863, 0.863, 0.863);
// static const Color COLOR_WHITE_SMOKE(0.961, 0.961, 0.961);
static const Color COLOR_WHITE(1, 1, 1);
};  // namespace esphome
