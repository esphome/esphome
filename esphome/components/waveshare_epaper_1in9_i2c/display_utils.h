#pragma once

#include "esphome/core/helpers.h"
#include "display_constants.h"

namespace esphome {
namespace waveshare_epaper_1in9_i2c {

static inline uint8_t get_pixel(int number, int order) { return number < 0 ? CHAR_EMPTY : CHAR_DIGITS[number][order]; }

}  // namespace waveshare_epaper_1in9_i2c
}  // namespace esphome
