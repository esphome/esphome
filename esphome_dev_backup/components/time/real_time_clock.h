#pragma once

#include <bitset>
#include <cstdlib>
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/time.h"

namespace esphome {
namespace time {

/// The RealTimeClock class exposes common timekeeping functions via the device's local real-time clock.
///
/// \note
/// The C library (newlib) available on ESPs only supports TZ strings that specify an offset and DST info;
/// you cannot specify zone names or paths to zoneinfo files.
/// \see https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
class RealTimeClock : public PollingComponent {
 public:
  explicit RealTimeClock();

  /// Set the time zone.
  void set_timezone(const std::string &tz) {
    this->timezone_ = tz;
    this->apply_timezone_();
  }

  /// Get the time zone currently in use.
  std::string get_timezone() { return this->timezone_; }

  /// Get the time in the currently defined timezone.
  ESPTime now() { return ESPTime::from_epoch_local(this->timestamp_now()); }

  /// Get the time without any time zone or DST corrections.
  ESPTime utcnow() { return ESPTime::from_epoch_utc(this->timestamp_now()); }

  /// Get the current time as the UTC epoch since January 1st 1970.
  time_t timestamp_now() { return ::time(nullptr); }

  void add_on_time_sync_callback(std::function<void()> callback) {
    this->time_sync_callback_.add(std::move(callback));
  };

 protected:
  /// Report a unix epoch as current time.
  void synchronize_epoch_(uint32_t epoch);

  std::string timezone_{};
  void apply_timezone_();

  CallbackManager<void()> time_sync_callback_;
};

template<typename... Ts> class TimeHasTimeCondition : public Condition<Ts...> {
 public:
  TimeHasTimeCondition(RealTimeClock *parent) : parent_(parent) {}
  bool check(Ts... x) override { return this->parent_->now().is_valid(); }

 protected:
  RealTimeClock *parent_;
};

}  // namespace time
}  // namespace esphome
