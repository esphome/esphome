#pragma once

#include "esphome/core/helpers.h"
#include "display_constants.h"

namespace esphome {
namespace waveshare_epaper_1in9_i2c {

static HOT void parse_number(float number, int *digits, int digits_count) {
  for (int i = 0; i < digits_count; i++) {
    if (isnanf(number)) {
      digits[i] = -1;
    } else {
      digits[i] = (int) (number / pow(10, (digits_count - i) - 2)) % 10;

      if (digits[i] == 0 && (i == 0 || digits[i - 1] == -1)) {
        digits[i] = -1;
      }
    }
  }
}

static uint8_t get_pixel(int number, int order) { return number < 0 ? CHAR_EMPTY : CHAR_DIGITS[number][order]; }

}  // namespace waveshare_epaper_1in9_i2c
}  // namespace esphome
