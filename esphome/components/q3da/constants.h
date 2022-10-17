#pragma once

#include <cstdint>

namespace esphome {
namespace q3da {
const uint8_t START_MASK = 47; // = / Start mark of a new telegram
const uint8_t END_MASK = 33; // = ! End mark of a new telegram
}  // namespace q3da
}  // namespace esphome
