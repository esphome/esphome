#pragma once
#include "esphome/core/color.h"

namespace esphome {
namespace display {
enum ColorOrder : uint8_t { COLOR_ORDER_RGB = 0, COLOR_ORDER_BGR = 1, COLOR_ORDER_GRB = 2 };
enum ColorBitness : uint8_t { COLOR_BITNESS_888 = 0, COLOR_BITNESS_565 = 1, COLOR_BITNESS_332 = 2 };
inline static uint8_t esp_scale(uint8_t i, uint8_t scale, uint8_t max_value = 255) { return (max_value * i / scale); }

class ColorUtil {
 public:
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
};
}  // namespace display
}  // namespace esphome
