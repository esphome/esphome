#pragma once

#include "esphome/core/helpers.h"

namespace esphome {
namespace textinput {

enum TextInputMode : uint8_t {
  TEXT_INPUT_MODE_AUTO = 0,
  TEXT_INPUT_MODE_STRING = 1,
//  TEXT_INPPUT_MODE_SECRET = 2,
};

class TextInputTraits {
 public:
  // Set/get the frontend mode.
  void set_mode(TextInputMode mode) { this->mode_ = mode; }
  TextInputMode get_mode() const { return this->mode_; }

 protected:
  TextInputMode mode_{TEXT_INPUT_MODE_AUTO};
};

}  // namespace textinput
}  // namespace esphome
