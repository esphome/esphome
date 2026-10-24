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
  virtual time_t timestamp_now() { return ::time(nullptr); }

  template<typename F> void add_on_time_sync_callback(F &&callback) {
    this->time_sync_callback_.add(std::forward<F>(callback));
  }

  void dump_config() override;

 protected:
  /// Report a unix epoch as current time.
  void synchronize_epoch_(uint32_t epoch);

  LazyCallbackManager<void()> time_sync_callback_;
};

/// How the weekday byte of a BCD calendar names the day, Sunday first.
enum class BcdWeekday : uint8_t {
  ONE_HOT,  ///< one bit per day, bit 0 is Sunday
  FROM_1,   ///< 1 is Sunday
  FROM_0,   ///< 0 is Sunday
};

/// Layout of a 7-byte BCD calendar: second, minute, hour, weekday and day of the month in either order,
/// month and year since 2000.
struct BcdCalendarLayout {
  BcdWeekday weekday;
  bool weekday_before_day{true};
  /// Bit 7 of the month byte is set from the year 2100 on. Chips without the bit read it as 0.
  bool century_bit{false};
  /// Bit 6 of the hour byte selects 12-hour mode, with PM in bit 5; decoded only, encoding writes 24-hour mode.
  bool hour_12h{false};

  constexpr size_t weekday_index() const { return this->weekday_before_day ? 3 : 4; }
  constexpr size_t day_index() const { return this->weekday_before_day ? 4 : 3; }
};

/// Epson RX8025T, RX8111, RX8130.
inline constexpr BcdCalendarLayout EPSON_BCD_CALENDAR{.weekday = BcdWeekday::ONE_HOT};
/// DS1307, DS3231.
inline constexpr BcdCalendarLayout DS_BCD_CALENDAR{
    .weekday = BcdWeekday::FROM_1, .century_bit = true, .hour_12h = true};
/// PCF8563, PCF85063, BM8563.
inline constexpr BcdCalendarLayout PCF_BCD_CALENDAR{.weekday = BcdWeekday::FROM_0, .weekday_before_day = false};

/// Decode the 7-byte BCD calendar at @p raw in the given layout and fill in the UTC timestamp. Bits each byte
/// does not use for its value, such as a chip's flag bits, are dropped; day_of_year is not worked out.
/// Kept out of line so that drivers sharing a layout share one copy.
template<BcdCalendarLayout LAYOUT> __attribute__((noinline)) ESPTime decode_bcd_calendar(const uint8_t *raw) {
  uint8_t day_of_week;
  if constexpr (LAYOUT.weekday == BcdWeekday::ONE_HOT) {
    const uint8_t weekday = raw[LAYOUT.weekday_index()] & 0x7F;
    day_of_week = weekday ? __builtin_ctz(weekday) + 1 : 1;
  } else if constexpr (LAYOUT.weekday == BcdWeekday::FROM_1) {
    day_of_week = raw[LAYOUT.weekday_index()] & 0x07;
  } else {
    day_of_week = (raw[LAYOUT.weekday_index()] & 0x07) + 1;
  }
  uint8_t hour = bcd_to_uint8(raw[2] & 0x3F);
  if constexpr (LAYOUT.hour_12h) {
    if ((raw[2] & 0x40) != 0) {
      // 12-hour mode, e.g. set by another library: 12 AM reads as 12 with PM clear, 12 PM as 12 with PM set.
      hour = bcd_to_uint8(raw[2] & 0x1F) % 12 + ((raw[2] & 0x20) != 0 ? 12 : 0);
    }
  }
  uint16_t year = 2000 + bcd_to_uint8(raw[6]);
  if constexpr (LAYOUT.century_bit) {
    if ((raw[5] & 0x80) != 0)
      year += 100;
  }
  ESPTime time{
      .second = bcd_to_uint8(raw[0] & 0x7F),
      .minute = bcd_to_uint8(raw[1] & 0x7F),
      .hour = hour,
      .day_of_week = day_of_week,
      .day_of_month = bcd_to_uint8(raw[LAYOUT.day_index()] & 0x3F),
      .day_of_year = 1,
      .month = bcd_to_uint8(raw[5] & 0x1F),
      .year = year,
      .is_dst = false,
      .timestamp = 0,
  };
  time.recalc_timestamp_utc(false);
  return time;
}

/// Encode @p time into the 7-byte BCD calendar at @p raw in the given layout, always in 24-hour mode.
/// @p time must be valid (is_valid()); the year must be 2000 to 2099 unless the layout has a century bit.
template<BcdCalendarLayout LAYOUT>
__attribute__((noinline)) void encode_bcd_calendar(const ESPTime &time, uint8_t *raw) {
  uint8_t value[7] = {time.second, time.minute, time.hour, 0, 0, time.month, static_cast<uint8_t>(time.year % 100)};
  value[LAYOUT.day_index()] = time.day_of_month;
  for (size_t i = 0; i < 7; i++)
    raw[i] = uint8_to_bcd(value[i]);
  if constexpr (LAYOUT.weekday == BcdWeekday::ONE_HOT) {
    raw[LAYOUT.weekday_index()] = 1 << (time.day_of_week - 1);
  } else if constexpr (LAYOUT.weekday == BcdWeekday::FROM_1) {
    raw[LAYOUT.weekday_index()] = time.day_of_week;
  } else {
    raw[LAYOUT.weekday_index()] = time.day_of_week - 1;
  }
  if constexpr (LAYOUT.century_bit) {
    if (time.year >= 2100)
      raw[5] |= 0x80;
  }
}

}  // namespace esphome::time
