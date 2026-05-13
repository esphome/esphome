#include "esphome/core/color.h"

namespace esphome {

// C++20 constinit ensures compile-time initialization (stored in ROM)
constinit const Color Color::BLACK(0, 0, 0, 0);
constinit const Color Color::WHITE(255, 255, 255, 255);

Color Color::gradient(const Color &to_color, uint8_t amnt) {
  uint8_t inv = 255 - amnt;
  Color new_color;
  new_color.r = (uint16_t(this->r) * inv + uint16_t(to_color.r) * amnt) / 255;
  new_color.g = (uint16_t(this->g) * inv + uint16_t(to_color.g) * amnt) / 255;
  new_color.b = (uint16_t(this->b) * inv + uint16_t(to_color.b) * amnt) / 255;
  new_color.w = (uint16_t(this->w) * inv + uint16_t(to_color.w) * amnt) / 255;
  return new_color;
}

Color Color::fade_to_white(uint8_t amnt) { return this->gradient(Color::WHITE, amnt); }

Color Color::fade_to_black(uint8_t amnt) { return this->gradient(Color::BLACK, amnt); }

}  // namespace esphome
