#pragma once
#include <array>
#include <cstdint>
#include "esphome/core/color.h"
#include "esphome/core/helpers.h"

namespace esphome::display {
enum ColorOrder : uint8_t { COLOR_ORDER_RGB = 0, COLOR_ORDER_BGR = 1, COLOR_ORDER_GRB = 2 };
enum ColorBitness : uint8_t { COLOR_BITNESS_888 = 0, COLOR_BITNESS_565 = 1, COLOR_BITNESS_332 = 2 };
inline static uint8_t esp_scale(uint8_t i, uint8_t scale, uint8_t max_value = 255) { return (max_value * i / scale); }

/// esp_scale(value, (1 << bits) - 1) for a field of 2, 3, 5, 6 or 8 bits, as a multiply and shift that gives the
/// same result for every field value. With bits a constant this avoids a division.
constexpr uint8_t expand_field(uint8_t value, uint8_t bits) {
  switch (bits) {
    case 2:
      return (value * 170) >> 1;
    case 3:
      return (value * 583) >> 4;
    case 5:
      return (value * 1053) >> 7;
    case 6:
      return (value * 4145) >> 10;
    case 8:
      return value;
    default:
      return esp_scale(value, (1 << bits) - 1);
  }
}

// Remove before 2027.5.0, with the forms that take a color order. Maps a color read in RGB order to the
// given order, by moving its channels.
inline Color apply_color_order(Color color, ColorOrder order) {
  switch (order) {
    case COLOR_ORDER_BGR:
      return Color(color.b, color.g, color.r);
    case COLOR_ORDER_GRB:
      return Color(color.g, color.r, color.b);
    default:
      return color;
  }
}

class ColorUtil {
 public:
  /// Reads one packed pixel of the given bitness from a source buffer into the
  /// value to_color() takes. BITNESS is a template argument so the byte layout
  /// folds; pass it on to to_color() as a constant too, and its channel scaling
  /// divides by a constant, which the compiler turns into a multiply and shift.
  template<ColorBitness BITNESS>
  static ESPHOME_ALWAYS_INLINE uint32_t read_packed(const uint8_t *ptr, size_t index, bool big_endian) {
    if constexpr (BITNESS == COLOR_BITNESS_565) {
      const size_t idx = index * 2;
      if (big_endian)
        return (ptr[idx] << 8) + ptr[idx + 1];
      return ptr[idx] + (ptr[idx + 1] << 8);
    } else if constexpr (BITNESS == COLOR_BITNESS_888) {
      // Always the LVGL RGB888 layout, b,g,r in memory; byte order applies to 565 only
      const size_t idx = index * 3;
      return ptr[idx + 0] + (ptr[idx + 1] << 8) + (ptr[idx + 2] << 16);
    } else {
      return ptr[index];
    }
  }

  /// Unpacks a color of the given bitness, red in the most significant bits.
  static Color to_color(uint32_t colorcode, ColorBitness color_bitness = ColorBitness::COLOR_BITNESS_888,
                        bool right_bit_aligned = true) {
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

    const uint8_t red =
        right_bit_aligned
            ? expand_field((colorcode >> (second_bits + third_bits)) & ((1 << first_bits) - 1), first_bits)
            : esp_scale(((colorcode >> 16) & 0xFF), (1 << first_bits) - 1);

    const uint8_t green = right_bit_aligned
                              ? expand_field((colorcode >> third_bits) & ((1 << second_bits) - 1), second_bits)
                              : esp_scale(((colorcode >> 8) & 0xFF), ((1 << second_bits) - 1));

    const uint8_t blue = right_bit_aligned ? expand_field(colorcode & ((1 << third_bits) - 1), third_bits)
                                           : esp_scale(((colorcode >> 0) & 0xFF), (1 << third_bits) - 1);

    return Color(red, green, blue);
  }
  // Remove before 2027.5.0
  ESPDEPRECATED("Packed colors are always RGB, drop the ColorOrder argument. Removed in 2027.5.0", "2026.11.0")
  static Color to_color(uint32_t colorcode, ColorOrder color_order,
                        ColorBitness color_bitness = ColorBitness::COLOR_BITNESS_888, bool right_bit_aligned = true) {
    return apply_color_order(to_color(colorcode, color_bitness, right_bit_aligned), color_order);
  }
  static inline Color rgb332_to_color(uint8_t rgb332_color) {
    return to_color((uint32_t) rgb332_color, COLOR_BITNESS_332);
  }
  static uint8_t color_to_332(Color color) {
    const uint16_t red_color = esp_scale8(color.red, ((1 << 3) - 1));
    const uint16_t green_color = esp_scale8(color.green, ((1 << 3) - 1));
    const uint16_t blue_color = esp_scale8(color.blue, (1 << 2) - 1);
    return red_color << 5 | green_color << 2 | blue_color;
  }
  // Remove before 2027.5.0
  ESPDEPRECATED("Packed colors are always RGB, drop the ColorOrder argument. Removed in 2027.5.0", "2026.11.0")
  static uint8_t color_to_332(Color color, ColorOrder color_order) {
    if (color_order == COLOR_ORDER_RGB)
      return color_to_332(color);
    // The other orders move the 2 bit field, so they cannot reuse the RGB packing
    const uint16_t red_color = esp_scale8(color.red, ((1 << 3) - 1));
    const uint16_t green_color = esp_scale8(color.green, ((1 << 3) - 1));
    if (color_order == COLOR_ORDER_BGR)
      return esp_scale8(color.blue, (1 << 2) - 1) << 6 | green_color << 3 | red_color;
    return green_color << 5 | red_color << 2 | esp_scale8(color.blue, (1 << 2) - 1);
  }
  static uint16_t color_to_565(Color color) {
    const uint16_t red_color = esp_scale8(color.red, ((1 << 5) - 1));
    const uint16_t green_color = esp_scale8(color.green, ((1 << 6) - 1));
    const uint16_t blue_color = esp_scale8(color.blue, (1 << 5) - 1);
    return red_color << 11 | green_color << 5 | blue_color;
  }
  // Remove before 2027.5.0
  ESPDEPRECATED("Packed colors are always RGB, drop the ColorOrder argument. Removed in 2027.5.0", "2026.11.0")
  static uint16_t color_to_565(Color color, ColorOrder color_order) {
    switch (color_order) {
      case COLOR_ORDER_BGR:
        return color_to_565(Color(color.b, color.g, color.r));
      case COLOR_ORDER_GRB:
        // Green moves to the 5 bit field at the top, so it cannot reuse the RGB packing
        return esp_scale8(color.green, (1 << 6) - 1) << 10 | esp_scale8(color.red, (1 << 5) - 1) << 5 |
               esp_scale8(color.blue, (1 << 5) - 1);
      default:
        return color_to_565(color);
    }
  }
  static uint32_t color_to_grayscale4(Color color) {
    uint32_t gs4 = esp_scale8(color.white, 15);
    return gs4;
  }
  /***
   * Converts a Color value to an 8bit index using a 24bit 888 palette.
   * Uses euclidiean distance to calculate the linear distance between
   * two points in an RGB cube, then iterates through the full palette
   * returning the closest match.
   * @param[in] color The target color.
   * @param[in] palette The 256*3 byte RGB palette.
   * @return The 8 bit index of the closest color (e.g. for display buffer).
   */
  // static uint8_t color_to_index8_palette888(Color color, uint8_t *palette) {
  static uint8_t color_to_index8_palette888(Color color, const uint8_t *palette) {
    uint8_t closest_index = 0;
    uint32_t minimum_dist2 = UINT32_MAX;  // Smallest distance^2 to the target
                                          // so far
    // int8_t(*plt)[][3] = palette;
    int16_t tgt_r = color.r;
    int16_t tgt_g = color.g;
    int16_t tgt_b = color.b;
    uint16_t x, y, z;
    // Loop through each row of the palette
    for (uint16_t i = 0; i < 256; i++) {
      // Get the pallet rgb color
      int16_t plt_r = (int16_t) palette[i * 3 + 0];
      int16_t plt_g = (int16_t) palette[i * 3 + 1];
      int16_t plt_b = (int16_t) palette[i * 3 + 2];
      // Calculate euclidean distance (linear distance in rgb cube).
      x = (uint32_t) std::abs(tgt_r - plt_r);
      y = (uint32_t) std::abs(tgt_g - plt_g);
      z = (uint32_t) std::abs(tgt_b - plt_b);
      uint32_t dist2 = x * x + y * y + z * z;
      if (dist2 < minimum_dist2) {
        minimum_dist2 = dist2;
        closest_index = (uint8_t) i;
      }
    }
    return closest_index;
  }
  /***
   * Converts an 8bit palette index (e.g. from a display buffer) to a color.
   * @param[in] index The index to look up.
   * @param[in] palette The 256*3 byte RGB palette.
   * @return The RGBW Color object looked up by the palette.
   */
  static Color index8_to_color_palette888(uint8_t index, const uint8_t *palette) {
    Color color = Color(palette[index * 3 + 0], palette[index * 3 + 1], palette[index * 3 + 2], 0);
    return color;
  }
};

/** Rec.601 luma weights (0.299/0.587/0.114), scaled by 256 */
static constexpr uint16_t REC601_LUMA_R = 77;
static constexpr uint16_t REC601_LUMA_G = 150;
static constexpr uint16_t REC601_LUMA_B = 29;

/** Rec.601 luma of an RGB color, for optimum perceptual brightness */
constexpr uint8_t rec601_luma(Color color) {
  return (uint8_t) ((REC601_LUMA_R * color.r + REC601_LUMA_G * color.g + REC601_LUMA_B * color.b + 128u) >> 8);
}

/** Map an RGB color to a single monochrome bit: 1 = white, 0 = black */
constexpr uint8_t color_to_mono(Color color) { return rec601_luma(color) >= 128 ? 1 : 0; }

/** color_to_mono() of an RGB565 value as ColorUtil::to_color() reads it, from table lookups instead of a division
 * per channel.
 */
class Rgb565ToMono {
 public:
  Rgb565ToMono() { this->fill_tables_(REC601_LUMA_R, REC601_LUMA_G, REC601_LUMA_B); }
  // Remove before 2027.5.0
  ESPDEPRECATED("Packed colors are always RGB, drop the ColorOrder argument. Removed in 2027.5.0", "2026.11.0")
  explicit Rgb565ToMono(ColorOrder order) {
    // The weight of the channel each 5, 6 and 5 bit field holds in this order
    const Color weights = apply_color_order(Color(REC601_LUMA_R, REC601_LUMA_G, REC601_LUMA_B), order);
    this->fill_tables_(weights.r, weights.g, weights.b);
  }

  uint8_t operator()(uint16_t value) const {
    const uint32_t weighted = this->high_[value >> 11] + this->middle_[(value >> 5) & 0x3F] + this->low_[value & 0x1F];
    return ((weighted + 128u) >> 8) >= 128 ? 1 : 0;
  }

 protected:
  // Each field value scaled to 0..255, then weighted
  void fill_tables_(uint16_t high, uint16_t middle, uint16_t low) {
    for (unsigned i = 0; i != 32; i++) {
      this->high_[i] = high * esp_scale(i, 31);
      this->low_[i] = low * esp_scale(i, 31);
    }
    for (unsigned i = 0; i != 64; i++)
      this->middle_[i] = middle * esp_scale(i, 63);
  }

  // Weighted contribution of each value of each field
  std::array<uint16_t, 32> high_{};
  std::array<uint16_t, 64> middle_{};
  std::array<uint16_t, 32> low_{};
};

}  // namespace esphome::display
