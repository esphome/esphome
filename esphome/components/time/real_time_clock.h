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
/// The C library (newlib) available on ESPs only supports TZ strings that specify an offset and DST info;
/// you cannot specify zone names or paths to zoneinfo files.
/// \see https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
class RealTimeClock : public PollingComponent {
 public:
  explicit RealTimeClock();

#ifdef USE_TIME_TIMEZONE
  /// Set the time zone from a POSIX TZ string.
  void set_timezone(const char *tz) { this->apply_timezone_(tz); }

  /// Set the time zone from a character buffer with known length.
  /// The buffer does not need to be null-terminated.
  void set_timezone(const char *tz, size_t len) {
    if (tz == nullptr) {
      this->apply_timezone_(nullptr);
      return;
    }
    // Stack buffer - TZ strings from tzdata are typically short (< 50 chars)
    char buf[128];
    if (len >= sizeof(buf))
      len = sizeof(buf) - 1;
    memcpy(buf, tz, len);
    buf[len] = '\0';
    this->apply_timezone_(buf);
  }

  /// Set the time zone from a std::string.
  void set_timezone(const std::string &tz) { this->apply_timezone_(tz.c_str()); }
#endif

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

#ifdef USE_TIME_TIMEZONE
  void apply_timezone_(const char *tz);
#endif

  LazyCallbackManager<void()> time_sync_callback_;
};

template<typename... Ts> class TimeHasTimeCondition : public Condition<Ts...> {
 public:
  TimeHasTimeCondition(RealTimeClock *parent) : parent_(parent) {}
  bool check(const Ts &...x) override { return this->parent_->now().is_valid(); }

 protected:
  RealTimeClock *parent_;
};

}  // namespace esphome::time
