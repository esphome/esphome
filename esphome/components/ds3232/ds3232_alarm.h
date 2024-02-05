#pragma once
#include <string>

namespace esphome {
namespace ds3232 {
namespace ds3232_alarm {

#define ORDINAL_SUFFIX(NUMBER) \
  ((((NUMBER) % 10) == 1) && (NUMBER) != 11) ? "st" \
                                             : (((((NUMBER) % 10) == 2) && (NUMBER) != 12)     ? "nd" \
                                                : (((((NUMBER) % 10) == 3) && (NUMBER) != 13)) ? "rd" \
                                                                                               : "th")

/// @brief Mode of the alarm
enum AlarmMode {
  /// Fire alarm once a second for
  /// alarm one and once per minute for
  /// alarm two.
  EVERY_TIME = 0b01111,

  /// Alarm when seconds match
  /// If seconds are not supported by alarm then
  /// it will be fired on every 00 seconds i.e. every minute.
  MATCH_SECONDS = 0b01110,

  /// Alarm when minutes and optionally seconds match
  MATCH_MINUTES_SECONDS = 0b01100,

  /// Alarm when hours, minutes, and optionally seconds match
  MATCH_TIME = 0b01000,

  /// Alarm when date, hours, minutes, and optionally seconds match
  MATCH_DATE_AND_TIME = 0b00000,

  /// Alarm when day, hours, minutes, and optionally seconds match
  MATCH_DAY_OF_WEEK_AND_TIME = 0b10000,
};

/// static AlarmMode bits_to_mode(bool use_week, bool match_day, bool match_hour, bool match_min, bool match_sec =
/// false);

/// Mode of alarm (firing pattern)
union DS3232AlarmMode {
  mutable struct {
    bool match_seconds : 1;
    bool match_minutes : 1;
    bool match_hours : 1;
    bool match_days : 1;
    bool use_weekdays : 1;
    uint8_t unused : 3;
  } bits;
  mutable AlarmMode alarm_mode;
  mutable uint8_t raw;
};


/// @brief Alarm description
struct DS3232Alarm {
  /// Indicates whether this alarm is enabled or not
  bool enabled = false;

  /// Determines whether seconds matching is supported
  /// by alarm. Default is true.
  bool seconds_supported = true;

  /// Mode of alarm (firing pattern)
  DS3232AlarmMode mode = alarm_mode(AlarmMode::MATCH_DATE_AND_TIME);

  /// On which second alarm should be fired
  uint8_t second = 0;

  /// On which minute alarm should be fired
  uint8_t minute = 0;

  /// On which hour alarm should be fired
  uint8_t hour = 0;

  /// On which day of week (1 is Sunday, 7 is Saturday) alarm should be fired
  uint8_t day_of_week = 1;

  /// On which day of month alarm should be fired.
  uint8_t day_of_month = 1;

  /// Indicates whether alarm has been fired
  bool is_fired = false;

  /// Checks whether alarm configuration is valid.
  /// @return true is alarm contains valid configuration; otherwise - false.
  bool is_valid() const;

  /// Formats alarm into human-readable text.
  /// @return Textual description of alarm.
std::string to_string() const;

  /// @brief Sets target alarm mode
  /// @param target_mode Mode of alarm to set
  void set_mode(AlarmMode target_mode) { mode.alarm_mode = target_mode; }

static DS3232AlarmMode alarm_mode(AlarmMode mode);
static DS3232AlarmMode alarm_mode(bool bit_seconds = false, bool bit_minutes = false,
                                  bool bit_hours = false, bool bit_days = false,
                                  bool use_weekdays = false);

static DS3232Alarm create(bool is_enabled, DS3232AlarmMode mode, bool use_weekdays = true,
                                uint8_t day = 1, uint8_t hour = 0, uint8_t minute = 0,
                                uint8_t second = 0, bool is_fired = false,
                                bool is_seconds_supported = false);
};

}  // namespace ds3232_alarm
}  // namespace ds3232
}  // namespace esphome
