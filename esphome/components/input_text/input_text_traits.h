#pragma once

#include "esphome/core/helpers.h"

namespace esphome {
namespace input_text {

enum InputTextMode : uint8_t {
  INPUT_TEXT_MODE_AUTO = 0,
  INPUT_TEXT_MODE_STRING = 1,
  INPUT_TEXT_MODE_PASSWORD = 2,
};

class InputTextTraits {
 public:
  // Set/get the frontend mode.
  void set_mode(InputTextMode mode) { this->mode_ = mode; }
  InputTextMode get_mode() const { return this->mode_; }
//  bool is_mode(auto check_mode){
//
//  }

 protected:
  InputTextMode mode_{INPUT_TEXT_MODE_AUTO};
};

}  // namespace input_text
}  // namespace esphome
