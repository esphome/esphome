#include "esphome/core/color.h"

namespace esphome {

// C++20 constinit ensures compile-time initialization (stored in ROM)
constinit const Color Color::BLACK(0, 0, 0, 0);
constinit const Color Color::WHITE(255, 255, 255, 255);

Color Color::gradient(const Color &to_color, uint8_t amnt) const {
  return Color(blend_channel(this->r, to_color.r, amnt), blend_channel(this->g, to_color.g, amnt),
               blend_channel(this->b, to_color.b, amnt), blend_channel(this->w, to_color.w, amnt));
}

Color Color::fade_to_white(uint8_t amnt) const { return this->gradient(Color::WHITE, amnt); }

Color Color::fade_to_black(uint8_t amnt) const { return this->gradient(Color::BLACK, amnt); }

}  // namespace esphome
