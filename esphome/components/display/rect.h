#pragma once

#include "esphome/core/helpers.h"

namespace esphome {
namespace display {

static const int16_t VALUE_NO_SET = 32766;

class Rect {
 public:
  int16_t x;  ///< X coordinate of corner
  int16_t y;  ///< Y coordinate of corner
  int16_t w;  ///< Width of region
  int16_t h;  ///< Height of region

  Rect() : x(VALUE_NO_SET), y(VALUE_NO_SET), w(VALUE_NO_SET), h(VALUE_NO_SET) {}  // NOLINT
  inline Rect(int16_t x, int16_t y, int16_t w, int16_t h) ESPHOME_ALWAYS_INLINE : x(x), y(y), w(w), h(h) {}
  inline int16_t x2() const { return this->x + this->w; };  ///< X coordinate of corner
  inline int16_t y2() const { return this->y + this->h; };  ///< Y coordinate of corner

  inline bool is_set() const ESPHOME_ALWAYS_INLINE { return (this->h != VALUE_NO_SET) && (this->w != VALUE_NO_SET); }

  void expand(int16_t horizontal, int16_t vertical);

  void extend(Rect rect);
  void shrink(Rect rect);

  bool inside(Rect rect) const;
  bool inside(int16_t test_x, int16_t test_y, bool absolute = true) const;
  bool equal(Rect rect) const;
  void info(const std::string &prefix = "rect info:");
};

}  // namespace display
}  // namespace esphome
