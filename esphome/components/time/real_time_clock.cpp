#include "real_time_clock.h"
#include "esphome/core/log.h"
#ifdef USE_HOST
#include <sys/time.h>
#else
#include "lwip/opt.h"
#endif
#ifdef USE_ESP8266
#include "sys/time.h"
#endif
#ifdef USE_RP2040
#include <sys/time.h>
#endif
#include <cerrno>

#include <cinttypes>

namespace esphome {
namespace time {

static const char *const TAG = "time";

RealTimeClock::RealTimeClock() = default;
void RealTimeClock::call_setup() {
  this->apply_timezone_();
  PollingComponent::call_setup();
}
void RealTimeClock::synchronize_epoch_(uint32_t epoch) {
  // Update UTC epoch time.
  struct timeval timev {
    .tv_sec = static_cast<time_t>(epoch), .tv_usec = 0,
  };
  ESP_LOGVV(TAG, "Got epoch %" PRIu32, epoch);
  struct timezone tz = {0, 0};
  int ret = settimeofday(&timev, &tz);
  if (ret == EINVAL) {
    // Some ESP8266 frameworks abort when timezone parameter is not NULL
    // while ESP32 expects it not to be NULL
    ret = settimeofday(&timev, nullptr);
  }

  // Move timezone back to local timezone.
  this->apply_timezone_();

  if (ret != 0) {
    ESP_LOGW(TAG, "setimeofday() failed with code %d", ret);
  }

  auto time = this->now();
  ESP_LOGD(TAG, "Synchronized time: %04d-%02d-%02d %02d:%02d:%02d", time.year, time.month, time.day_of_month, time.hour,
           time.minute, time.second);

  this->time_sync_callback_.call();
}

void RealTimeClock::apply_timezone_() {
  setenv("TZ", this->timezone_.c_str(), 1);
  tzset();
}

void RealTimeClock::set_time(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second,
                             bool utc) {
  ESPTime time{
      .second = second,
      .minute = minute,
      .hour = hour,
      .day_of_week = 1,  // not used
      .day_of_month = day,
      .day_of_year = 1,  // ignored by recalc_timestamp_utc(false)
      .month = month,
      .year = year,
      .is_dst = false,  // not used
      .timestamp = 0    // overwritten by recalc_timestamp_utc(false)
  };

  if (utc) {
    time.recalc_timestamp_utc();
  } else {
    time.recalc_timestamp_local();
  }

  if (!time.is_valid()) {
    ESP_LOGE(TAG, "Invalid time, not syncing to system clock.");
    return;
  }
  time::RealTimeClock::synchronize_epoch_(time.timestamp);
};

void RealTimeClock::set_time(ESPTime datetime, bool utc) {
  return this->set_time(datetime.year, datetime.month, datetime.day_of_month, datetime.hour, datetime.minute,
                        datetime.second, utc);
};

void RealTimeClock::set_time(const std::string &datetime, bool utc) {
  ESPTime val{};
  if (!ESPTime::strptime(datetime, val)) {
    ESP_LOGE(TAG, "Could not convert the time string to an ESPTime object");
    return;
  }
  this->set_time(val, utc);
}

void RealTimeClock::set_time(time_t epoch_seconds, bool utc) {
  ESPTime val = ESPTime::from_epoch_local(epoch_seconds);
  this->set_time(val, utc);
}

}  // namespace time
}  // namespace esphome
