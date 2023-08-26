#ifdef USE_ARDUINO

#include "optolink_text_sensor.h"
#include "../optolink.h"
#include "esphome/components/api/api_server.h"
#include "VitoWiFi.h"
#include <iostream>
#include <sstream>
#include <vector>

namespace esphome {
namespace optolink {

static const char *const TAG = "optolink.text_sensor";

struct Time {
  int hours;
  int minutes;
};

bool check_time_sequence(const Time &t1, const Time &t2) {
  if (t2.hours > t1.hours || (t2.hours == t1.hours && t2.minutes >= t1.minutes)) {
    return true;
  }
  return false;
}

bool check_time_values(const Time &time) {
  return (time.hours >= 0 && time.hours <= 23) && (time.minutes >= 0 && time.minutes <= 59);
}

uint8_t *encode_time_string(std::string input) {
  std::istringstream iss(input);
  std::vector<Time> time_values;

  Time prev_time = {0, 0};

  while (iss) {
    std::string time_string;
    iss >> time_string;

    if (time_string.empty()) {
      break;
    }

    Time current_time;
    if (sscanf(time_string.c_str(), "%d:%d", &current_time.hours, &current_time.minutes) == 2) {
      if (check_time_values(current_time) && check_time_sequence(prev_time, current_time)) {
        time_values.push_back(current_time);
        prev_time = current_time;
      } else {
        ESP_LOGE(
            TAG,
            "Time values should be in the format hh:mm and in increasing order within the range of 00:00 to 23:59");
        return 0;
      }
    } else {
      ESP_LOGE(TAG, "Invalid time format");
      return 0;
    }
  }
  if (time_values.size() > 8) {
    ESP_LOGE(TAG, "Maximum 8 time values allowed");
    return 0;
  }
  if (time_values.size() % 2) {
    ESP_LOGE(TAG, "Number of time values must be even");
    return 0;
  }

  while (time_values.size() < 8) {
    time_values.push_back({31, 70});
  }

  static uint8_t data[8];
  // ESP_LOGD(TAG, "Parsed time values:");
  for (int i = 0; i < 8; i++) {
    Time time = time_values[i];
    data[i] = (time.hours << 3) + (time.minutes / 10);
    // ESP_LOGD(TAG, "  %02d:%02d => %d", time.hours, time.minutes, data[i]);
  }

  return data;
}

void OptolinkTextSensor::setup() {
  if (mode_ == RAW) {
    div_ratio_ = 0;
  } else if (mode_ == DAY_SCHEDULE) {
    div_ratio_ = 0;
    bytes_ = 8;
    address_ += (8 * dow_);
  } else if (mode_ == DAY_SCHEDULE_SYNCHRONIZED) {
    writeable_ = true;
    div_ratio_ = 0;
    bytes_ = 8;
    address_ += (8 * dow_);
    api::global_api_server->subscribe_home_assistant_state(
        this->entity_id_, optional<std::string>(), [this](const std::string &state) {
          ESP_LOGD(TAG, "got time values from entity '%s': %s", this->entity_id_.c_str(), state.c_str());
          uint8_t *data = encode_time_string(state);
          if (data) {
            update_datapoint(data, 8);
          } else {
            ESP_LOGW(TAG, "not changing any value of datapoint %s", datapoint_->getName());
          }
        });
  }
  setup_datapoint();
};

void OptolinkTextSensor::value_changed(uint8_t *state, size_t length) {
  switch (mode_) {
    case RAW:
      publish_state(std::string((const char *) state));
      break;
    case DAY_SCHEDULE:
    case DAY_SCHEDULE_SYNCHRONIZED:
      if (length == 8) {
        char buffer[6 * length + 1];
        for (int i = 0; i < 8; i++) {
          int hour = state[i] >> 3;
          int minute = (state[i] & 0b111) * 10;
          if (state[i] != 0xFF) {
            sprintf(buffer + i * 6, "%02d:%02d ", hour, minute);
          } else {
            sprintf(buffer + i * 6, "      ");
          }
        }
        publish_state(buffer);
      } else {
        unfitting_value_type();
      }
      break;
    case MAP:
      unfitting_value_type();
      break;
  }
};

}  // namespace optolink
}  // namespace esphome

#endif
