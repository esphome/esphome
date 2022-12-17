#pragma once

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
  // Set/get the frontend mode.
  void set_mode(TextMode mode) { this->mode_ = mode; }
  TextMode get_mode() const { return this->mode_; }

 protected:
  TextMode mode_{TEXT_MODE_AUTO};
};

}  // namespace text
}  // namespace esphome
