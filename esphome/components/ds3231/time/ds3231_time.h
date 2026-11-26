#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/time/real_time_clock.h"

#include "../ds3231.h"

namespace esphome::ds3231 {

class DS3231Time final : public time::RealTimeClock, public Parented<DS3231Component> {
 public:
  void update() override;
  void dump_config() override;

  /// Set the system clock from the RTC.
  void read_time();
  /// Store the system clock in the RTC.
  void write_time();
};

}  // namespace esphome::ds3231
