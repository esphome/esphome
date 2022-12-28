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
  void set_min(int min) { min_ = min; }
  int get_min() const { return min_; }
  void set_max(int max) { max_ = max; }
  int get_max() const { return max_; }

  // Set/get the pattern.
  void set_pattern(std::string pattern) { pattern_ = std::move(pattern); }
  std::string get_pattern() const { return pattern_; }

  // Set/get the frontend mode.
  void set_mode(TextMode mode) { this->mode_ = mode; }
  TextMode get_mode() const { return this->mode_; }

 protected:
  int min_;
  int max_;
  std::string pattern_;
  TextMode mode_{TEXT_MODE_AUTO};
};

}  // namespace text
}  // namespace esphome
