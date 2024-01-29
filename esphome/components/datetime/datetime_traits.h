#pragma once

#include <utility>
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace datetime {

enum DatetimeMode : uint8_t {
  DATETIME_MODE_AUTO = 0,
};

class DatetimeTraits : public EntityBase_DeviceClass {
 public:
  // Set/get the frontend mode.
  void set_mode(DatetimeMode mode) { this->mode_ = mode; }
  DatetimeMode get_mode() const { return this->mode_; }

 protected:
  DatetimeMode mode_{DATETIME_MODE_AUTO};
};

}  // namespace datetime
}  // namespace esphome
