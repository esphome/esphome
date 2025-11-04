// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2023 Ilia Sotnikov

#pragma once
#include <vector>
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/time/automation.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace dynamic_on_time {

class DynamicOnTime : public Component {
 public:
  explicit DynamicOnTime(
    time::RealTimeClock *, number::Number *, number::Number *,
    switch_::Switch *, switch_::Switch *, switch_::Switch *, switch_::Switch *,
    switch_::Switch *, switch_::Switch *, switch_::Switch *, switch_::Switch *,
    std::vector<esphome::Action<> *>);

  void setup() override;
  void dump_config() override;

  optional<ESPTime> get_next_schedule();

 protected:
  time::RealTimeClock *rtc_;
  number::Number *hour_;
  number::Number *minute_;
  switch_::Switch *mon_;
  switch_::Switch *tue_;
  switch_::Switch *wed_;
  switch_::Switch *thu_;
  switch_::Switch *fri_;
  switch_::Switch *sat_;
  switch_::Switch *sun_;
  switch_::Switch *disabled_;
  std::vector<esphome::Action<> *> actions_;
  time::CronTrigger *trigger_{nullptr};
  Automation<> *automation_{nullptr};
  std::vector<uint8_t> days_of_week_{};

  std::vector<uint8_t> flags_to_days_of_week_(
    bool, bool, bool, bool, bool, bool, bool);

  void update_schedule_();
  optional<ESPTime> next_schedule_{};
};

}  // namespace dynamic_on_time
}  // namespace esphome
