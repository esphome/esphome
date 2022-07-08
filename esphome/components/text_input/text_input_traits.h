#pragma once

#include "esphome/core/helpers.h"

namespace esphome {
namespace text_input {

enum TextInputMode : uint8_t {
  TEXT_INPUT_MODE_AUTO = 0,
  TEXT_INPUT_MODE_STRING = 1,
  TEXT_INPUT_MODE_PASSWORD = 2,
};

class TextInputTraits {
 public:
  // Set/get the frontend mode.
  void set_mode(TextInputMode mode) { this->mode_ = mode; }
  TextInputMode get_mode() const { return this->mode_; }
//  bool is_mode(auto check_mode){
//
//  }

 protected:
  TextInputMode mode_{TEXT_INPUT_MODE_AUTO};
};

}  // namespace textinput
}  // namespace esphome
