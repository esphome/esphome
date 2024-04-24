#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/time.h"

#include "real_time_clock.h"

#include <vector>

namespace esphome {
namespace time {

class CronTrigger : public Trigger<>, public Component {
 public:
  explicit CronTrigger(RealTimeClock *rtc);
  void add_second(uint8_t second);
  void add_seconds(const std::vector<uint8_t> &seconds);
  void add_minute(uint8_t minute);
  void add_minutes(const std::vector<uint8_t> &minutes);
  void add_hour(uint8_t hour);
  void add_hours(const std::vector<uint8_t> &hours);
  void add_day_of_month(uint8_t day_of_month);
  void add_days_of_month(const std::vector<uint8_t> &days_of_month);
  void add_month(uint8_t month);
  void add_months(const std::vector<uint8_t> &months);
  void add_day_of_week(uint8_t day_of_week);
  void add_days_of_week(const std::vector<uint8_t> &days_of_week);
  bool matches(const ESPTime &time);
  void loop() override;
  float get_setup_priority() const override;

 protected:
  std::bitset<61> seconds_;
  std::bitset<60> minutes_;
  std::bitset<24> hours_;
  std::bitset<32> days_of_month_;
  std::bitset<13> months_;
  std::bitset<8> days_of_week_;
  RealTimeClock *rtc_;
  optional<ESPTime> last_check_;
};

class SyncTrigger : public Trigger<>, public Component {
 public:
  explicit SyncTrigger(RealTimeClock *rtc);

 protected:
  RealTimeClock *rtc_;
};
}  // namespace time
}  // namespace esphome
