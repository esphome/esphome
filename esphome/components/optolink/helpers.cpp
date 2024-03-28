#ifdef USE_ARDUINO

#include "helpers.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace optolink {

static const char *const TAG = "optolink.helpers";

void rtrim(std::string &s) {
  if (s.empty())
    return;

  std::string::iterator p;
  for (p = s.end(); p != s.begin() && *--p == ' ';)
    ;

  if (*p != ' ')
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

}  // namespace optolink
}  // namespace esphome

#endif
