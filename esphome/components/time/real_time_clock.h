#pragma once

#include <bitset>
#include <cstdlib>
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/time.h"
#ifdef USE_TIME_TIMEZONE
#include "posix_tz.h"
#endif

namespace esphome::time {

/// The RealTimeClock class exposes common timekeeping functions via the device's local real-time clock.
///
/// \note
/// The timezone is pre-parsed into a ParsedTimezone struct: at codegen time from the YAML
/// configuration, or at runtime by API clients that send the parsed struct (Home Assistant
/// 2026.3.0 and newer). See set_global_tz() in posix_tz.h.
class RealTimeClock : public PollingComponent {
 public:
  explicit RealTimeClock();

  /// Get the time in the currently defined timezone.
  ESPTime now();

  /// Get the time without any time zone or DST corrections.
  ESPTime utcnow() { return ESPTime::from_epoch_utc(this->timestamp_now()); }

  /// Get the current time as the UTC epoch since January 1st 1970.
  time_t timestamp_now() { return ::time(nullptr); }

  template<typename F> void add_on_time_sync_callback(F &&callback) {
    this->time_sync_callback_.add(std::forward<F>(callback));
  }

  void dump_config() override;

 protected:
  /// Report a unix epoch as current time.
  void synchronize_epoch_(uint32_t epoch);

  LazyCallbackManager<void()> time_sync_callback_;
};

template<typename... Ts> class TimeHasTimeCondition final : public Condition<Ts...> {
 public:
  TimeHasTimeCondition(RealTimeClock *parent) : parent_(parent) {}
  bool check(const Ts &...x) override { return this->parent_->now().is_valid(); }

 protected:
  RealTimeClock *parent_;
};

}  // namespace esphome::time
