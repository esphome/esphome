#pragma once

#include "esphome/core/defines.h"

#ifdef USE_DATETIME_DATE

#include "esphome/components/datetime/date_entity.h"
#include "esphome/core/component.h"

namespace esphome {
namespace demo {

class DemoDate : public datetime::DateEntity, public Component {
 public:
  void setup() override {
    this->year_ = 2038;
    this->month_ = 01;
    this->day_ = 19;
    this->publish_state();
  }

 protected:
  void control(const datetime::DateCall &call) override {
    this->year_ = call.get_year().value_or(this->year_);
    this->month_ = call.get_month().value_or(this->month_);
    this->day_ = call.get_day().value_or(this->day_);
    this->publish_state();
  }
};

}  // namespace demo
}  // namespace esphome

#endif
