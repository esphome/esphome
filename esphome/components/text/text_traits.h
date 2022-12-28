#pragma once

#include <utility>

#include "esphome/core/helpers.h"

namespace esphome {
namespace text {

enum TextMode : uint8_t {
  TEXT_MODE_AUTO = 0,
  TEXT_MODE_TEXT = 1,
  TEXT_MODE_PASSWORD = 2,
};

class TextTraits {
 public:
  // Set/get the number value boundaries.
  void set_min_value(int min_value) { min_value_ = min_value; }
  int get_min_value() const { return min_value_; }
  void set_max_value(int max_value) { max_value_ = max_value; }
  int get_max_value() const { return max_value_; }

  // Set/get the pattern.
  void set_pattern(std::string pattern) { pattern_ = std::move(pattern); }
  std::string get_pattern() const { return pattern_; }

  // Set/get the frontend mode.
  void set_mode(TextMode mode) { this->mode_ = mode; }
  TextMode get_mode() const { return this->mode_; }

 protected:
  int min_value_;
  int max_value_;
  std::string pattern_;
  TextMode mode_{TEXT_MODE_AUTO};
};

}  // namespace text
}  // namespace esphome
