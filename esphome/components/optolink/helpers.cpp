#ifdef USE_ARDUINO

#include "helpers.h"
#include "esphome/core/log.h"
#include <cstring>
#include <ctime>
#include <algorithm>

namespace esphome {
namespace optolink {

static const char *const TAG = "optolink.helpers";

void rtrim(std::string &s) {
  if (s.empty())
    return;

  std::string::iterator p;
  p = std::find_if(s.rbegin(), s.rend(), [](char ch) { return ch != ' '; }).base();

  if (p != s.begin() && *p != ' ')
    p++;

  s.erase(p, s.end());
}

std::string decode_day_schedule(const uint8_t *input) {
  char buffer[49];
  for (int i = 0; i < 8; i++) {
    int hour = input[i] >> 3;
    int minute = (input[i] & 0b111) * 10;
    if (input[i] != 0xFF) {
      sprintf(buffer + i * 6, "%02d:%02d ", hour, minute);
    } else {
      sprintf(buffer + i * 6, "      ");
    }
  }
  return std::string(buffer);
}

uint8_t *encode_day_schedule(const std::string &input, uint8_t *output) {
  char buffer[49];
  strncpy(buffer, input.c_str(), sizeof(buffer));
  buffer[sizeof(buffer) - 1] = 0x00;
  Time time_values[8];
  Time prev_time = {0, 0};
  int time_count = 0;

  char *token = strtok(buffer, " ");
  while (token && time_count < 8) {
    Time current_time;
    // NOLINTNEXTLINE
    if (sscanf(token, "%d:%d", &current_time.hours, &current_time.minutes) == 2) {
      if (check_time_values(current_time) && check_time_sequence(prev_time, current_time)) {
        time_values[time_count++] = current_time;
        prev_time = current_time;
      } else {
        ESP_LOGE(
            TAG,
            "Time values should be in the format hh:mm and in increasing order within the range of 00:00 to 23:59");
        return nullptr;
      }
    } else {
      ESP_LOGE(TAG, "Invalid time format");
      return nullptr;
    }
    token = strtok(nullptr, " ");
  }

  if (time_count % 2) {
    ESP_LOGE(TAG, "Number of time values must be even");
    return nullptr;
  }

  while (time_count < 8) {
    time_values[time_count++] = {31, 70};
  }

  for (int i = 0; i < 8; i++) {
    Time time = time_values[i];
    output[i] = (time.hours << 3) + (time.minutes / 10);
  }

  return output;
}

/**
 * @brief Encodes a datetime string in ISO 8601 format into a BCD-encoded buffer.
 *
 * This function parses a datetime string in the format "YYYY-MM-DDTHH:MM:SS" and encodes its components
 * (year, month, day, weekday, hour, minute, second) into a buffer using Binary-Coded Decimal (BCD) encoding.
 * The weekday is calculated such that Monday=1 and Sunday=7.
 *
 * @param value   The datetime string to encode (must be exactly 19 characters, in "YYYY-MM-DDTHH:MM:SS" format).
 * @param buffer  Pointer to an array of at least 8 bytes where the encoded data will be stored.
 *                The buffer will be filled as follows:
 *                  buffer[0]: BCD-encoded century (e.g., 20 for 2023)
 *                  buffer[1]: BCD-encoded year within century (e.g., 23 for 2023)
 *                  buffer[2]: BCD-encoded month (1-12)
 *                  buffer[3]: BCD-encoded day (1-31)
 *                  buffer[4]: BCD-encoded weekday (Monday=1, ..., Sunday=7)
 *                  buffer[5]: BCD-encoded hour (0-23)
 *                  buffer[6]: BCD-encoded minute (0-59)
 *                  buffer[7]: BCD-encoded second (0-59)
 * @return true if the input string was successfully parsed and encoded, false otherwise.
 */
bool encode_datetime(const std::string &value, uint8_t *buffer) {
  if (value.length() != 19 || value[10] != 'T') {
    ESP_LOGW(TAG, "Invalid format: Expected length 19 and 'T' at position 10.");
    return false;
  }

  int year, month, day, hour, min, sec;
  if (sscanf(value.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day, &hour, &min, &sec) != 6) {
    ESP_LOGE(TAG, "Failed to parse datetime string: %s", value.c_str());
    return false;
  }

  struct tm tm = {0};
  tm.tm_year = year - 1900;
  tm.tm_mon = month - 1;
  tm.tm_mday = day;
  tm.tm_hour = hour;
  tm.tm_min = min;
  tm.tm_sec = sec;

  if (mktime(&tm) == -1) {
    ESP_LOGE(TAG, "Failed to convert time using mktime.");
    return false;
  }

  buffer[0] = dec_to_bcd(year / 100);
  buffer[1] = dec_to_bcd(year % 100);
  buffer[2] = dec_to_bcd(month);
  buffer[3] = dec_to_bcd(day);
  buffer[4] = dec_to_bcd((tm.tm_wday + 6) % 7 + 1);
  buffer[5] = dec_to_bcd(hour);
  buffer[6] = dec_to_bcd(min);
  buffer[7] = dec_to_bcd(sec);

  ESP_LOGD(TAG, "Encoded buffer values: %02X %02X %02X %02X %02X %02X %02X %02X", buffer[0], buffer[1], buffer[2],
           buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]);
  return true;
}

/**
 * @brief Decodes from a BCD-encoded buffer to a datetime string in ISO 8601 format.
 *
 * This function interprets an 8-byte buffer containing BCD-encoded date and time values,
 * and returns a formatted ISO 8601 datetime string (YYYY-MM-DDTHH:MM:SS).
 *
 * @param buffer Pointer to the buffer containing the BCD-encoded datetime.
 * @param length Length of the buffer; must be exactly 8 bytes.
 * @return A string representing the decoded datetime in ISO 8601 format,
 *         or "Invalid" if the buffer length is not 8.
 */
std::string decode_datetime(const uint8_t *buffer, size_t length) {
  if (length != 8) {
    ESP_LOGW(TAG, "Invalid buffer length: %d. Expected length: 8.", length);
    return "Invalid";
  }

  int year = bcd_to_dec(buffer[0]) * 100 + bcd_to_dec(buffer[1]);
  int month = bcd_to_dec(buffer[2]);
  int day = bcd_to_dec(buffer[3]);
  int hour = bcd_to_dec(buffer[5]);
  int min = bcd_to_dec(buffer[6]);
  int sec = bcd_to_dec(buffer[7]);
  char buf[26];
  int written = snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d", year, month, day, hour, min, sec);

  if (written < 0 || written >= static_cast<int>(sizeof(buf))) {
    ESP_LOGE(TAG, "Failed to format datetime string. Buffer size: %d, Written: %d", sizeof(buf), written);
    return "Invalid";
  }

  ESP_LOGD(TAG, "Formatted datetime string: %s", buf);
  return std::string(buf);
}

}  // namespace optolink
}  // namespace esphome

#endif
