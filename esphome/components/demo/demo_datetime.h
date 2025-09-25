#pragma once

#include "esphome/core/defines.h"

#ifdef USE_DATETIME_DATETIME

#include "esphome/components/datetime/datetime_entity.h"
#include "esphome/core/component.h"

namespace esphome {
namespace demo {

class DemoDateTime : public datetime::DateTimeEntity, public Component {
 public:
  void setup() override {
    this->year_ = 2038;
    this->month_ = 01;
    this->day_ = 19;
    this->hour_ = 3;
    this->minute_ = 14;
    this->second_ = 8;
    this->publish_state();
  }

 protected:
  void control(const datetime::DateTimeCall &call) override {
    this->year_ = call.get_year().value_or(this->year_);
    this->month_ = call.get_month().value_or(this->month_);
    this->day_ = call.get_day().value_or(this->day_);
    this->hour_ = call.get_hour().value_or(this->hour_);
    this->minute_ = call.get_minute().value_or(this->minute_);
    this->second_ = call.get_second().value_or(this->second_);
    this->publish_state();
  }
};

}  // namespace demo
}  // namespace esphome

#endif
