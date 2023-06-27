#include "real_time_clock.h"
#include "esphome/core/log.h"
#include "lwip/opt.h"
#ifdef USE_ESP8266
#include "sys/time.h"
#endif
#ifdef USE_RP2040
#include <sys/time.h>
#endif
#include <cerrno>

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
  ESP_LOGVV(TAG, "Got epoch %u", epoch);
  timezone tz = {0, 0};
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

}  // namespace time
}  // namespace esphome
