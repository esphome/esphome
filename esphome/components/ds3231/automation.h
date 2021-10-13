#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "ds3231.h"

namespace esphome {
namespace ds3231 {

template<typename... Ts> class SetAlarm1Action : public Action<Ts...>, public Parented<DS3231Component> {
 public:
  TEMPLATABLE_VALUE(DS3231Alarm1Type, alarm_type)
  TEMPLATABLE_VALUE(int, second)
  TEMPLATABLE_VALUE(int, minute)
  TEMPLATABLE_VALUE(int, hour)
  TEMPLATABLE_VALUE(int, day)

  void play(Ts... x) override {
    this->parent_->set_alarm_1(this->alarm_type_.value(x...), this->second_.value(x...), this->minute_.value(x...),
                               this->hour_.value(x...), this->day_.value(x...));
  }
};

template<typename... Ts> class ResetAlarm1Action : public Action<Ts...>, public Parented<DS3231Component> {
 public:
  void play(Ts... x) override { this->parent_->reset_alarm_1(); }
};

template<typename... Ts> class SetAlarm2Action : public Action<Ts...>, public Parented<DS3231Component> {
 public:
  TEMPLATABLE_VALUE(DS3231Alarm2Type, alarm_type)
  TEMPLATABLE_VALUE(int, minute)
  TEMPLATABLE_VALUE(int, hour)
  TEMPLATABLE_VALUE(int, day)

  void play(Ts... x) override {
    this->parent_->set_alarm_2(this->alarm_type_.value(x...), this->minute_.value(x...), this->hour_.value(x...),
                               this->day_.value(x...));
  }
};

template<typename... Ts> class ResetAlarm2Action : public Action<Ts...>, public Parented<DS3231Component> {
 public:
  void play(Ts... x) override { this->parent_->reset_alarm_2(); }
};

template<typename... Ts> class WriteTimeAction : public Action<Ts...>, public Parented<DS3231RTC> {
 public:
  void play(Ts... x) override { this->parent_->write_time(); }
};

template<typename... Ts> class ReadTimeAction : public Action<Ts...>, public Parented<DS3231RTC> {
 public:
  void play(Ts... x) override { this->parent_->read_time(); }
};

}  // namespace ds3231
}  // namespace esphome
