#include "esphome/core/color.h"

namespace esphome {

// C++20 constinit ensures compile-time initialization (stored in ROM)
constinit const Color Color::BLACK(0, 0, 0, 0);
constinit const Color Color::WHITE(255, 255, 255, 255);

}  // namespace esphome
