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

namespace esphome::time {

static const char *const TAG = "time";

RealTimeClock::RealTimeClock() = default;

void RealTimeClock::dump_config() {
#ifdef USE_TIME_TIMEZONE
  int std_hours = -this->parsed_tz_.std_offset_seconds / 3600;
  int std_mins = abs(this->parsed_tz_.std_offset_seconds % 3600) / 60;
  ESP_LOGCONFIG(TAG, "Timezone: UTC%+d:%02d", std_hours, std_mins);
  if (this->parsed_tz_.has_dst) {
    int dst_hours = -this->parsed_tz_.dst_offset_seconds / 3600;
    char start_buf[24], end_buf[24];
    internal::format_dst_rule(this->parsed_tz_.dst_start, start_buf);
    internal::format_dst_rule(this->parsed_tz_.dst_end, end_buf);
    ESP_LOGCONFIG(TAG, "  DST: UTC%+d, %s - %s", dst_hours, start_buf, end_buf);
  }
#endif
  auto time = this->now();
  ESP_LOGCONFIG(TAG, "Current time: %04d-%02d-%02d %02d:%02d:%02d", time.year, time.month, time.day_of_month, time.hour,
                time.minute, time.second);
}

void RealTimeClock::synchronize_epoch_(uint32_t epoch) {
  ESP_LOGVV(TAG, "Got epoch %" PRIu32, epoch);
  // Skip if time is already synchronized to avoid unnecessary writes, log spam,
  // and prevent clock jumping backwards due to network latency
  constexpr time_t min_valid_epoch = 1546300800;  // January 1, 2019
  time_t current_time = this->timestamp_now();
  // Check if time is valid (year >= 2019) before comparing
  if (current_time >= min_valid_epoch) {
    // Unsigned subtraction handles wraparound correctly, then cast to signed
    int32_t diff = static_cast<int32_t>(epoch - static_cast<uint32_t>(current_time));
    if (diff >= -1 && diff <= 1) {
      // Time is already synchronized, but still call callbacks so components
      // waiting for time sync (e.g., uptime timestamp sensor) can initialize
      this->time_sync_callback_.call();
      return;
    }
  }
  // Update UTC epoch time.
#ifdef USE_ZEPHYR
  struct timespec ts;
  ts.tv_nsec = 0;
  ts.tv_sec = static_cast<time_t>(epoch);

  int ret = clock_settime(CLOCK_REALTIME, &ts);

  if (ret != 0) {
    ESP_LOGW(TAG, "clock_settime() failed with code %d", ret);
  }
#else
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

  if (ret != 0) {
    ESP_LOGW(TAG, "setimeofday() failed with code %d", ret);
  }
#endif
  auto time = this->now();
  ESP_LOGD(TAG, "Synchronized time: %04d-%02d-%02d %02d:%02d:%02d", time.year, time.month, time.day_of_month, time.hour,
           time.minute, time.second);

  this->time_sync_callback_.call();
}

#ifdef USE_TIME_TIMEZONE
void RealTimeClock::apply_timezone_(const char *tz) {
  // Handle null input
  if (tz == nullptr) {
    ESP_LOGW(TAG, "Failed to parse timezone: (null)");
    this->parsed_tz_ = ParsedTimezone{};
    return;
  }

  // Parse the POSIX TZ string using our custom parser
  if (!parse_posix_tz(tz, this->parsed_tz_)) {
    ESP_LOGW(TAG, "Failed to parse timezone: %s", tz);
    // Reset to UTC on parse failure
    this->parsed_tz_ = ParsedTimezone{};
  }
}
#endif

}  // namespace esphome::time
