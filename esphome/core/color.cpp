#include "esphome/core/color.h"

namespace esphome {

// C++20 constinit ensures compile-time initialization (stored in ROM)
constinit const Color Color::BLACK(0, 0, 0, 0);
constinit const Color Color::WHITE(255, 255, 255, 255);

Color Color::gradient(const Color &to_color, uint8_t amnt) {
  Color new_color;
  float amnt_f = float(amnt) / 255.0f;
  new_color.r = amnt_f * (to_color.r - this->r) + this->r;
  new_color.g = amnt_f * (to_color.g - this->g) + this->g;
  new_color.b = amnt_f * (to_color.b - this->b) + this->b;
  new_color.w = amnt_f * (to_color.w - this->w) + this->w;
  return new_color;
}

Color Color::fade_to_white(uint8_t amnt) { return this->gradient(Color::WHITE, amnt); }

Color Color::fade_to_black(uint8_t amnt) { return this->gradient(Color::BLACK, amnt); }

}  // namespace esphome
