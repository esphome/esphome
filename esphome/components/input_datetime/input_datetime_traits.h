#pragma once

#include <utility>
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace datetime {

enum InputDatetimeMode : uint8_t {
  INPUT_DATETIME_MODE_AUTO = 0,
};

class InputDatetimeTraits : public EntityBase_DeviceClass {
 public:
  // Set/get the frontend mode.
  void set_mode(InputDatetimeMode mode) { this->mode_ = mode; }
  InputDatetimeMode get_mode() const { return this->mode_; }

 protected:
  InputDatetimeMode mode_{INPUT_DATETIME_MODE_AUTO};
};

}  // namespace datetime
}  // namespace esphome
