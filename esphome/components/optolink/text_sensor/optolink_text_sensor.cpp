#ifdef USE_ARDUINO

#include "esphome/core/log.h"
#include "optolink_text_sensor.h"
#include "../optolink.h"
#include "../datapoint_component.h"

namespace esphome {
namespace optolink {

static const char *const TAG = "optolink.text_sensor";

struct Time {
  int hours;
  int minutes;
};

bool check_time_sequence(const Time &t1, const Time &t2) {
  return t2.hours > t1.hours || (t2.hours == t1.hours && t2.minutes >= t1.minutes);
}

bool check_time_values(const Time &time) {
  return (time.hours >= 0 && time.hours <= 23) && (time.minutes >= 0 && time.minutes <= 59);
}

uint8_t *encode_time_string(const std::string input) {
  char buffer[49];
  strncpy(buffer, input.c_str(), sizeof(buffer));
  buffer[sizeof(buffer) - 1] = 0x00;
  Time time_values[8];
  Time prev_time = {0, 0};
  int time_count = 0;

  char *token = strtok(buffer, " ");
  while (token && time_count < 8) {
    Time current_time;
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

  static uint8_t data[8];
  for (int i = 0; i < 8; i++) {
    Time time = time_values[i];
    data[i] = (time.hours << 3) + (time.minutes / 10);
  }

  return data;
}

void OptolinkTextSensor::setup() {
  switch (mode_) {
    case MAP:
      break;
    case RAW:
      set_div_ratio(0);
      break;
    case DAY_SCHEDULE:
      set_div_ratio(0);
      set_bytes(8);
      set_address(get_address_() + 8 * dow_);
      break;
    case DAY_SCHEDULE_SYNCHRONIZED:
      set_writeable(true);
      set_div_ratio(0);
      set_bytes(8);
      set_address(get_address_() + 8 * dow_);
      ESP_LOGI(TAG, "subscribing to schedule plan from HASS entity '%s' for component %s", this->entity_id_.c_str(),
               get_component_name().c_str());
      subscribe_hass_(entity_id_, [this](const std::string &state) {
        ESP_LOGD(TAG, "update for schedule plan for component %s: %s", get_component_name().c_str(), state.c_str());
        uint8_t *data = encode_time_string(state);
        if (data) {
          write_datapoint_value_(data, 8);
        } else {
          ESP_LOGW(TAG, "not changing any value of datapoint %s", get_component_name().c_str());
        }
      });
      break;
    case DEVICE_INFO:
      set_entity_category(esphome::ENTITY_CATEGORY_DIAGNOSTIC);
      set_bytes(4);
      set_address(0x00f8);
      break;
    case STATE_INFO:
      set_entity_category(esphome::ENTITY_CATEGORY_DIAGNOSTIC);
      return;  // no datapoint setup!
  }
  setup_datapoint_();
};

void OptolinkTextSensor::update() {
  if (mode_ == STATE_INFO) {
    publish_state(get_optolink_state_());
  } else {
    datapoint_read_request_();
  }
}

void OptolinkTextSensor::datapoint_value_changed(uint8_t *value, size_t length) {
  switch (mode_) {
    case RAW:
      publish_state(std::string((const char *) value));
      break;
    case DAY_SCHEDULE:
    case DAY_SCHEDULE_SYNCHRONIZED:
      if (length == 8) {
        char buffer[6 * length + 1];
        for (int i = 0; i < 8; i++) {
          int hour = value[i] >> 3;
          int minute = (value[i] & 0b111) * 10;
          if (value[i] != 0xFF) {
            sprintf(buffer + i * 6, "%02d:%02d ", hour, minute);
          } else {
            sprintf(buffer + i * 6, "      ");
          }
        }
        publish_state(buffer);
      } else {
        unfitting_value_type_();
      }
      break;
    case DEVICE_INFO:
    case STATE_INFO:
    case MAP:
      unfitting_value_type_();
      break;
  }
};

void OptolinkTextSensor::datapoint_value_changed(uint32_t value) {
  switch (mode_) {
    case DEVICE_INFO: {
      uint8_t *bytes = (uint8_t *) &value;
      uint16_t tmp = esphome::byteswap(*((uint16_t *) bytes));
      std::string geraetekennung = esphome::format_hex_pretty(&tmp, 1);
      std::string hardware_revision = esphome::format_hex_pretty((uint8_t *) bytes + 2, 1);
      std::string software_index = esphome::format_hex_pretty((uint8_t *) bytes + 3, 1);
      publish_state("Device ID: " + geraetekennung + "|Hardware Revision: " + hardware_revision +
                    "|Software Index: " + software_index);
    } break;
    default:
      publish_state(std::to_string(value));
  }
};

}  // namespace optolink
}  // namespace esphome

#endif
