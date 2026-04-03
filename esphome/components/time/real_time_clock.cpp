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
#include <cstdlib>

namespace esphome::time {

static const char *const TAG = "time";

RealTimeClock::RealTimeClock() = default;

ESPTime __attribute__((noinline)) RealTimeClock::now() {
#ifdef USE_TIME_TIMEZONE
  time_t epoch = this->timestamp_now();
  struct tm local_tm;
  if (epoch_to_local_tm(epoch, get_global_tz(), &local_tm)) {
    return ESPTime::from_c_tm(&local_tm, epoch);
  }
  // Fallback to UTC if parsing failed
  return ESPTime::from_epoch_utc(epoch);
#else
  return ESPTime::from_epoch_local(this->timestamp_now());
#endif
}

void RealTimeClock::dump_config() {
#ifdef USE_TIME_TIMEZONE
  const auto &tz = get_global_tz();
  // POSIX offset is positive west, negate for conventional UTC+X display
  int std_h = -tz.std_offset_seconds / 3600;
  int std_m = (std::abs(tz.std_offset_seconds) % 3600) / 60;
  if (tz.has_dst()) {
    int dst_h = -tz.dst_offset_seconds / 3600;
    int dst_m = (std::abs(tz.dst_offset_seconds) % 3600) / 60;
    ESP_LOGCONFIG(TAG, "Timezone: UTC%+d:%02d (DST UTC%+d:%02d)", std_h, std_m, dst_h, dst_m);
  } else {
    ESP_LOGCONFIG(TAG, "Timezone: UTC%+d:%02d", std_h, std_m);
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
  if (ret != 0 && errno == EINVAL) {
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
  ParsedTimezone parsed{};

  // Handle null or empty input - use UTC
  if (tz == nullptr || *tz == '\0') {
    // Skip if already UTC
    if (!get_global_tz().has_dst() && get_global_tz().std_offset_seconds == 0) {
      return;
    }
    set_global_tz(parsed);
    return;
  }

#ifdef USE_HOST
  // On host platform, also set TZ environment variable for libc compatibility
  setenv("TZ", tz, 1);
  tzset();
#endif

  // Parse the POSIX TZ string using our custom parser
  if (!parse_posix_tz(tz, parsed)) {
    ESP_LOGW(TAG, "Failed to parse timezone: %s", tz);
    return;
  }

  // Set global timezone for all time conversions
  set_global_tz(parsed);
}
#endif

}  // namespace esphome::time
