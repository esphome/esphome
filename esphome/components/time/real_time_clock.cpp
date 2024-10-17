#include "real_time_clock.h"
#include "esphome/core/log.h"
#ifdef USE_HOST
#include <sys/time.h>
#elif defined(USE_ZEPHYR)
#include <zephyr/posix/time.h>
#else
#include "lwip/opt.h"
#endif
#ifdef USE_ESP8266
#include "sys/time.h"
#endif
#if defined(USE_RP2040) || defined(USE_ZEPHYR)
#include <sys/time.h>
#endif
#include <cerrno>

#include <cinttypes>

namespace esphome {
namespace time {

static const char *const TAG = "time";

RealTimeClock::RealTimeClock() = default;
void RealTimeClock::call_setup() {
#ifndef USE_ZEPHYR
  this->apply_timezone_();
#endif
  PollingComponent::call_setup();
}
void RealTimeClock::synchronize_epoch_(uint32_t epoch) {
  ESP_LOGVV(TAG, "Got epoch %" PRIu32, epoch);
  // Update UTC epoch time.
#ifndef USE_ZEPHYR
  struct timeval timev {
    .tv_sec = static_cast<time_t>(epoch), .tv_usec = 0,
  };
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
#else
  struct timespec ts;
  ts.tv_nsec = 0;
  ts.tv_sec = static_cast<time_t>(epoch);

  int ret = clock_settime(CLOCK_REALTIME, &ts);

  if (ret != 0) {
    ESP_LOGW(TAG, "clock_settime() failed with code %d", ret);
  }

#endif
  auto time = this->now();
  ESP_LOGD(TAG, "Synchronized time: %04d-%02d-%02d %02d:%02d:%02d", time.year, time.month, time.day_of_month, time.hour,
           time.minute, time.second);

  this->time_sync_callback_.call();
}

#ifndef USE_ZEPHYR
void RealTimeClock::apply_timezone_() {
  setenv("TZ", this->timezone_.c_str(), 1);
  tzset();
}
#endif

}  // namespace time
}  // namespace esphome
