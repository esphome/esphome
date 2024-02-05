#include "ds3232_alarm.h"
#include "esphome/core/log.h"
#include <array>

namespace esphome {
namespace ds3232 {
namespace ds3232_alarm {

static const char *const TAG = "ds3232_alarm";

static const char *const DAYS_OF_WEEK[] = {"", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

static const char *const INVALID = "invalid";
static const char *const FORMAT_STRING_EVERY_TIME_S = "every second";
static const char *const FORMAT_STRING_EVERY_TIME_M = "every minute";
static const char *const FORMAT_STRING_EVERY_MINUTE = "every minute on %.2u%s second";
static const char *const FORMAT_STRING_EVERY_HOUR = "every hour on %.2u:%.2u";
static const char *const FORMAT_STRING_EVERY_DAY = "every day on %.2u:%.2u:%.2u";
static const char *const FORMAT_STRING_EVERY_WEEK = "every %s on %.2u:%.2u:%.2u";
static const char *const FORMAT_STRING_EVERY_MONTH = "every %u%s day of month on %.2u:%.2u:%.2u";
static const char *const FORMAT_STRING_INVALID = INVALID;

// Possible output formatted strings for DS3232Alarm::to_string():
//  every 31st day of month on 23:59:59   35 symbols
//  every Mon on 23:59:59                 21 symbols
//  every day on 23:59:59                 21 symbols
//  every hour on 59:59                   19 symbols
//  every minute on 59th second           27 symbols
//  every minute                          12 symbols
//  every second                          12 symbols
//  disabled                               8 symbols
// So, the recommended formatted_string length initialization: 128 symbols (35 * 3 (105) growed to 2^x(128))
static const size_t DS3232_ALARM_FORMAT_STRING_LENGTH = 128;

/// Converts alarm match bits to mode
// static AlarmMode bits_to_mode(bool use_week, bool match_day, bool match_hour, bool match_min, bool match_sec = false) {
//   uint8_t mode_bits =
//       0x00001 * match_sec + 0x00010 * match_min + 0x00100 * match_hour + 0x01000 * match_day + 0x10000 * use_week;
//   return static_cast<AlarmMode>(mode_bits);
// };

DS3232AlarmMode DS3232Alarm::alarm_mode(const AlarmMode mode) {
  DS3232AlarmMode am = {};
  am.alarm_mode = mode;
  return am;
};

DS3232AlarmMode DS3232Alarm::alarm_mode(const bool bit_seconds, const bool bit_minutes, const bool bit_hours,
                                  const bool bit_days, const bool use_weekdays) {
  bool weekdays_bit = use_weekdays && !(bit_seconds || bit_minutes || bit_hours);
  uint8_t mode_bits = 0b00001 * bit_seconds + 0b00010 * bit_minutes + 0b00100 * bit_hours + 0b01000 * bit_days +
                      0b10000 * weekdays_bit;
  ESP_LOGVV(TAG, "Converted alarm mode from %.1u, %.1u, %.1u, %.1u, %.1u to bits %.2u",
    bit_seconds, bit_minutes, bit_hours, bit_days, use_weekdays, mode_bits);
  return DS3232Alarm::alarm_mode(static_cast<AlarmMode>(mode_bits));
};

bool DS3232Alarm::is_valid() const {
  bool result = true;
  result &= this->seconds_supported ? (this->second >= 0 && this->second < 60) : true;
  result &= this->minute >= 0 && this->minute < 60;
  result &= this->hour >= 0 && this->hour < 24;
  result &= this->day_of_week > 0 && this->day_of_week <= 7;
  result &= this->day_of_month > 0 && this->day_of_month <= 31;
  return result;
}

DS3232Alarm DS3232Alarm::create(const bool is_enabled, const DS3232AlarmMode mode, const bool use_weekdays,
                                const uint8_t day, const uint8_t hour, const uint8_t minute,
                                const uint8_t second, const bool is_fired,
                                const bool is_seconds_supported)
{
  DS3232Alarm alarm = {};
  //   .enabled = is_enabled,
  //   .seconds_supported = is_seconds_supported,
  //   .mode = mode,
  //   .second = second,
  //   .minute = minute,
  //   .hour = hour,
  //   .day_of_week = (use_weekdays ? day : 1),
  //   .day_of_month = (use_weekdays ? 1 : day),
  //   .is_fired = is_fired
  // };
  ESP_LOGVV(TAG, "Initializing new instance of alarm:");
  alarm.enabled = is_enabled;
  ESP_LOGVV(TAG, " Alarm state: %s", ONOFF(alarm.enabled));
  alarm.seconds_supported = is_seconds_supported;
  ESP_LOGVV(TAG, " Seconds supported: %s", YESNO(alarm.seconds_supported));
  alarm.mode = mode;
  ESP_LOGVV(TAG, " Mode set to: %.2u (%.2u)", alarm.mode, mode);
  alarm.second = second;
  ESP_LOGVV(TAG, " Seconds set to: %.2u (%.2u)", alarm.second, second);
  alarm.minute = minute;
  ESP_LOGVV(TAG, " Minutes set to: %.2u (%.2u)", alarm.minute, minute);
  alarm.hour = hour;
  ESP_LOGVV(TAG, " Hours set to: %.2u (%.2u)", alarm.hour, hour);
  alarm.day_of_week = use_weekdays ? day : 1;
  ESP_LOGVV(TAG, " Day of month set to: %.2u (%.2u)", alarm.day_of_week, use_weekdays ? day : 1);
  alarm.day_of_month = use_weekdays ? 1 : day;
  ESP_LOGVV(TAG, " Day of week set to: %.2u (%.2u)", alarm.day_of_month, use_weekdays ? 1 : day);
  alarm.is_fired = is_fired;
  ESP_LOGVV(TAG, " Alarm is fired", YESNO(alarm.is_fired));
  return alarm;
}

std::string DS3232Alarm::to_string() const {
  if (!this->is_valid())
        return std::string(INVALID);
  char formatted_string[DS3232_ALARM_FORMAT_STRING_LENGTH];
  size_t len;
  switch (this->mode.alarm_mode) {
    case AlarmMode::EVERY_TIME:
    case 31:
      return std::string(this->seconds_supported ? FORMAT_STRING_EVERY_TIME_S : FORMAT_STRING_EVERY_TIME_M);
    case AlarmMode::MATCH_SECONDS:
    case 30:
      if (!this->seconds_supported)
        return std::string(FORMAT_STRING_EVERY_TIME_M);
      len = snprintf(formatted_string, sizeof(formatted_string), FORMAT_STRING_EVERY_MINUTE, this->second,
                     ORDINAL_SUFFIX(this->second));
      if (len < 0) {
        ESP_LOGE(TAG, "Unable to format alarm data.");
        return std::string(INVALID);
      }
      return std::string(formatted_string);
    case AlarmMode::MATCH_MINUTES_SECONDS:
    case 28:
      len =
          snprintf(formatted_string, sizeof(formatted_string), FORMAT_STRING_EVERY_HOUR, this->minute, this->second);
      if (len < 0) {
        ESP_LOGE(TAG, "Unable to format alarm data.");
        return std::string(INVALID);
      }
      return std::string(formatted_string);
    case AlarmMode::MATCH_TIME:
    case 24:
      len = snprintf(formatted_string, sizeof(formatted_string), FORMAT_STRING_EVERY_DAY, this->hour, this->minute,
                     this->second);
      if (len < 0) {
        ESP_LOGE(TAG, "Unable to format alarm data.");
        return std::string(INVALID);
      }
      return std::string(formatted_string);
    case AlarmMode::MATCH_DAY_OF_WEEK_AND_TIME:
      len = snprintf(formatted_string, sizeof(formatted_string), FORMAT_STRING_EVERY_WEEK,
                     DAYS_OF_WEEK[this->day_of_week], this->hour, this->minute, this->second);
      if (len < 0) {
        ESP_LOGE(TAG, "Unable to format alarm data.");
        return std::string(INVALID);
      }
      return std::string(formatted_string);
    case AlarmMode::MATCH_DATE_AND_TIME:
      len = snprintf(formatted_string, sizeof(formatted_string), FORMAT_STRING_EVERY_MONTH, this->day_of_month,
                     ORDINAL_SUFFIX(this->day_of_month), this->hour, this->minute, this->second);
      if (len < 0) {
        ESP_LOGE(TAG, "Unable to format alarm data.");
        return std::string(INVALID);
      }
      return std::string(formatted_string);
    default:
      return INVALID;
      break;
  }
}

}  // namespace ds3232_alarm
}  // namespace ds3232
}  // namespace esphome
