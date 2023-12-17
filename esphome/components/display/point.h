#pragma once

#include "esphome/core/helpers.h"

namespace esphome {
namespace display {

class Point {
 public:
  int16_t x;
  int16_t y;

  Point() : x(VALUE_NO_SET), y(VALUE_NO_SET) {};
  inline Point(int16_t x, int16_t y) ALWAYS_INLINE : x(x), y(y) {};
};

}  // namespace display
}  // namespace esphome
