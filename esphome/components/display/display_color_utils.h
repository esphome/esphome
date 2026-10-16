#pragma once
#include <array>
#include <cstdint>
#include "esphome/core/color.h"
#include "esphome/core/helpers.h"

namespace esphome::display {
enum ColorOrder : uint8_t { COLOR_ORDER_RGB = 0, COLOR_ORDER_BGR = 1, COLOR_ORDER_GRB = 2 };
enum ColorBitness : uint8_t { COLOR_BITNESS_888 = 0, COLOR_BITNESS_565 = 1, COLOR_BITNESS_332 = 2 };
inline static uint8_t esp_scale(uint8_t i, uint8_t scale, uint8_t max_value = 255) { return (max_value * i / scale); }

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
      // Always the LVGL RGB888 layout, b,g,r in memory for RGB order; byte order applies to 565 only
      const size_t idx = index * 3;
      return ptr[idx + 0] + (ptr[idx + 1] << 8) + (ptr[idx + 2] << 16);
    } else {
      return ptr[index];
    }
  }

  static Color to_color(uint32_t colorcode, ColorOrder color_order,
                        ColorBitness color_bitness = ColorBitness::COLOR_BITNESS_888, bool right_bit_aligned = true) {
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

    third_color = (right_bit_aligned ? esp_scale(((colorcode >> 0) & ((1 << third_bits) - 1)), ((1 << third_bits) - 1))
                                     : esp_scale(((colorcode >> 0) & 0xFF), (1 << third_bits) - 1));

    Color color_return;

    switch (color_order) {
      case COLOR_ORDER_RGB:
        color_return.r = first_color;
        color_return.g = second_color;
        color_return.b = third_color;
        break;
      case COLOR_ORDER_BGR:
        color_return.b = first_color;
        color_return.g = second_color;
        color_return.r = third_color;
        break;
      case COLOR_ORDER_GRB:
        color_return.g = first_color;
        color_return.r = second_color;
        color_return.b = third_color;
        break;
    }
    return color_return;
  }
  static inline Color rgb332_to_color(uint8_t rgb332_color) {
    return to_color((uint32_t) rgb332_color, COLOR_ORDER_RGB, COLOR_BITNESS_332);
  }
  static uint8_t color_to_332(Color color, ColorOrder color_order = ColorOrder::COLOR_ORDER_RGB) {
    uint16_t red_color, green_color, blue_color;

    red_color = esp_scale8(color.red, ((1 << 3) - 1));
    green_color = esp_scale8(color.green, ((1 << 3) - 1));
    blue_color = esp_scale8(color.blue, (1 << 2) - 1);

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
  static uint16_t color_to_565(Color color, ColorOrder color_order = ColorOrder::COLOR_ORDER_RGB) {
    uint16_t red_color, green_color, blue_color;

    red_color = esp_scale8(color.red, ((1 << 5) - 1));
    green_color = esp_scale8(color.green, ((1 << 6) - 1));
    blue_color = esp_scale8(color.blue, (1 << 5) - 1);

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
  explicit Rgb565ToMono(ColorOrder order) {
    // Rec.601 weight of the 5-bit, 6-bit and 5-bit fields, by the channel each one holds in this order
    uint16_t high = REC601_LUMA_R, middle = REC601_LUMA_G, low = REC601_LUMA_B;
    if (order == COLOR_ORDER_BGR) {
      high = REC601_LUMA_B;
      low = REC601_LUMA_R;
    } else if (order == COLOR_ORDER_GRB) {
      high = REC601_LUMA_G;
      middle = REC601_LUMA_R;
    }
    // Each field value scaled to 0..255, then weighted
    for (unsigned i = 0; i != 32; i++) {
      this->high_[i] = high * esp_scale(i, 31);
      this->low_[i] = low * esp_scale(i, 31);
    }
    for (unsigned i = 0; i != 64; i++)
      this->middle_[i] = middle * esp_scale(i, 63);
  }

  uint8_t operator()(uint16_t value) const {
    const uint32_t weighted = this->high_[value >> 11] + this->middle_[(value >> 5) & 0x3F] + this->low_[value & 0x1F];
    return ((weighted + 128u) >> 8) >= 128 ? 1 : 0;
  }

 protected:
  // Weighted contribution of each value of each field
  std::array<uint16_t, 32> high_{};
  std::array<uint16_t, 64> middle_{};
  std::array<uint16_t, 32> low_{};
};

}  // namespace esphome::display
