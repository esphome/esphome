#pragma once

#include "esphome/core/defines.h"

#ifdef USE_DATETIME_TIME

#include "esphome/components/datetime/time_entity.h"
#include "esphome/core/component.h"

namespace esphome {
namespace demo {

class DemoTime : public datetime::TimeEntity, public Component {
 public:
  void setup() override {
    this->hour_ = 3;
    this->minute_ = 14;
    this->second_ = 8;
    this->publish_state();
  }

 protected:
  void control(const datetime::TimeCall &call) override {
    this->hour_ = call.get_hour().value_or(this->hour_);
    this->minute_ = call.get_minute().value_or(this->minute_);
    this->second_ = call.get_second().value_or(this->second_);
    this->publish_state();
  }
};

}  // namespace demo
}  // namespace esphome

#endif
