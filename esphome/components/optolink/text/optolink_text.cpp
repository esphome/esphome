#ifdef USE_ARDUINO

#include "esphome/core/log.h"
#include "optolink_text.h"
#include "../optolink.h"
#include "../datapoint_component.h"
#include "../helpers.h"

namespace esphome {
namespace optolink {

static const char *const TAG = "optolink.text";

void OptolinkText::setup() {
  switch (type_) {
    case TEXT_TYPE_DAY_SCHEDULE:
      set_writeable(true);
      set_div_ratio(DIV_RATIO_BINARY);
      set_bytes(8);
      set_address(get_address_() + 8 * dow_);
      traits.set_max_length(48);
      traits.set_pattern("(((([0-1]?[0-9]|2[0-3]):[0-5]0)( |$)){2})*");
      break;
    case TEXT_TYPE_DATETIME:
      set_writeable(true);
      set_div_ratio(DIV_RATIO_BINARY);
      set_bytes(8);
      traits.set_max_length(19);
      traits.set_pattern(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2})");
      break;
  }
  setup_datapoint_();
};

void OptolinkText::control(const std::string &value) {
  switch (type_) {
    case TEXT_TYPE_DAY_SCHEDULE: {
      ESP_LOGD(TAG, "update for schedule plan for component %s: %s", get_component_name().c_str(), value.c_str());
      uint8_t buffer[8];
      uint8_t *data = encode_day_schedule(value, buffer);
      if (data) {
        write_datapoint_value_(data, 8);
      } else {
        ESP_LOGW(TAG, "not changing any value of datapoint %s", get_component_name().c_str());
      }
      break;
    }
    case TEXT_TYPE_DATETIME: {
      ESP_LOGD(TAG, "update for datetime for component %s: %s", get_component_name().c_str(), value.c_str());
      uint8_t buffer[8];
      if (encode_datetime(value, buffer)) {
        write_datapoint_value_(buffer, 8);
      } else {
        ESP_LOGW(TAG, "Invalid datetime format for %s", get_component_name().c_str());
      }
      break;
    }
    default:
      ESP_LOGE(TAG, "Unknown type: %d", type_);
      break;
  }
}

void OptolinkText::datapoint_value_changed(uint8_t *value, size_t length) {
  switch (type_) {
    case TEXT_TYPE_DAY_SCHEDULE:
      if (length == 8) {
        auto schedule = decode_day_schedule(value);
        rtrim(schedule);
        publish_state(schedule);
      } else {
        unfitting_value_type_();
      }
      break;
    case TEXT_TYPE_DATETIME:
      if (length == 8) {
        auto datetime = decode_datetime(value, length);
        publish_state(datetime);
      } else {
        unfitting_value_type_();
      }
      break;
    default:
      ESP_LOGE(TAG, "Unknown type: %d", type_);
      break;
  }
};

}  // namespace optolink
}  // namespace esphome

#endif
