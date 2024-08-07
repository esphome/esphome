#pragma once

#include <utility>

#include "esphome/core/helpers.h"

namespace esphome {
namespace text {

enum TextMode : uint8_t {
  TEXT_MODE_TEXT = 0,
  TEXT_MODE_PASSWORD = 1,
};

class TextTraits {
 public:
  // Set/get the number value boundaries.
  void set_min_length(int min_length) { this->min_length_ = min_length; }
  int get_min_length() const { return this->min_length_; }
  void set_max_length(int max_length) { this->max_length_ = max_length; }
  int get_max_length() const { return this->max_length_; }

  // Set/get the pattern.
  void set_pattern(std::string pattern) { this->pattern_ = std::move(pattern); }
  std::string get_pattern() const { return this->pattern_; }

  // Set/get the frontend mode.
  void set_mode(TextMode mode) { this->mode_ = mode; }
  TextMode get_mode() const { return this->mode_; }

 protected:
  int min_length_;
  int max_length_;
  std::string pattern_;
  TextMode mode_{TEXT_MODE_TEXT};
};

}  // namespace text
}  // namespace esphome
