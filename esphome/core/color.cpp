#include "esphome/core/color.h"

namespace esphome {

// C++20 constinit ensures compile-time initialization (stored in ROM)
constinit const Color Color::BLACK(0, 0, 0, 0);
constinit const Color Color::WHITE(255, 255, 255, 255);
constinit const Color Color::RED(255, 0, 0, 0);
constinit const Color Color::GREEN(0, 255, 0, 0);
constinit const Color Color::BLUE(0, 0, 255, 0);
constinit const Color Color::YELLOW(255, 255, 0, 0);
constinit const Color Color::ORANGE(255, 166, 0, 0);

}  // namespace esphome
