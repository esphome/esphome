#include "real_time_clock.h"
#include "esphome/core/log.h"
#ifdef USE_HOST
#include <sys/time.h>
#elif defined(USE_ZEPHYR)
// Removed in sdk-nrf 3.3.0, where POSIX_API provides the time functions instead
#if __has_include(<zephyr/posix/time.h>)
#include <zephyr/posix/time.h>
#include <sys/time.h>
#endif
#else
#include "lwip/opt.h"
#endif
#ifdef USE_ESP8266
#include "sys/time.h"
#endif
#ifdef USE_RP2
#include <sys/time.h>
#endif
#include <cerrno>
#include <cinttypes>
#include <cstdlib>

namespace esphome::time {

ESPHOME_LOG_TAG(TAG, "time");

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
  struct timeval timev{
      .tv_sec = static_cast<time_t>(epoch),
      .tv_usec = 0,
  };
#ifdef USE_ESP8266
  // ESP8266 settimeofday() requires tz to be nullptr
  int ret = settimeofday(&timev, nullptr);
#else
  struct timezone tz = {0, 0};
  int ret = settimeofday(&timev, &tz);
#endif

  if (ret != 0) {
    ESP_LOGW(TAG, "settimeofday() failed with code %d", ret);
  }
#endif
  auto time = this->now();
  ESP_LOGD(TAG, "Synchronized time: %04d-%02d-%02d %02d:%02d:%02d", time.year, time.month, time.day_of_month, time.hour,
           time.minute, time.second);

  this->time_sync_callback_.call();
}

// Bits of each calendar byte that hold the value, byte 0 lowest; byte 3 is the one-hot weekday.
// Packed into one constant so it stays in code rather than RAM on ESP8266.
static constexpr uint64_t BCD_CALENDAR_MASKS = 0xFF1F3F7F3F7F7FULL;

ESPTime decode_bcd_calendar(const uint8_t *raw) {
  uint8_t value[7];
  for (size_t i = 0; i < 7; i++)
    value[i] = bcd_to_uint8(raw[i] & static_cast<uint8_t>(BCD_CALENDAR_MASKS >> (8 * i)));
  const uint8_t weekday = raw[3] & static_cast<uint8_t>(BCD_CALENDAR_MASKS >> 24);
  ESPTime time{
      .second = value[0],
      .minute = value[1],
      .hour = value[2],
      .day_of_week = static_cast<uint8_t>(weekday ? __builtin_ctz(weekday) + 1 : 1),
      .day_of_month = value[4],
      .day_of_year = 1,
      .month = value[5],
      .year = static_cast<uint16_t>(value[6] + 2000),
      .is_dst = false,
      .timestamp = 0,
  };
  time.recalc_timestamp_utc(false);
  return time;
}

void encode_bcd_calendar(const ESPTime &time, uint8_t *raw) {
  const uint8_t value[7] = {
      time.second, time.minute, time.hour, 0, time.day_of_month, time.month, static_cast<uint8_t>(time.year % 100)};
  for (size_t i = 0; i < 7; i++)
    raw[i] = uint8_to_bcd(value[i]);
  raw[3] = 1 << (time.day_of_week - 1);
}

}  // namespace esphome::time
