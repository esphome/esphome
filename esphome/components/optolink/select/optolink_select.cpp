#ifdef USE_ARDUINO

#include "optolink_select.h"
#include "../optolink.h"

namespace esphome {
namespace optolink {

static const char *const TAG = "optolink.select";

void OptolinkSelect::control(const std::string &value) {
  for (auto it = mapping_->begin(); it != mapping_->end(); ++it) {
    if (it->second == value) {
      ESP_LOGI(TAG, "control of select %s to value %s", get_component_name().c_str(), it->first.c_str());
      write_datapoint_value_(std::stof(it->first));
      publish_state(it->second);
      break;
    }
    if (it == mapping_->end()) {
      ESP_LOGE(TAG, "unknown value %s of select %s", value.c_str(), get_component_name().c_str());
    }
  }
};

void OptolinkSelect::datapoint_value_changed(const std::string &value) {
  auto pos = mapping_->find(value);
  if (pos == mapping_->end()) {
    ESP_LOGE(TAG, "value %s not found in select %s", value.c_str(), get_component_name().c_str());
  } else {
    publish_state(pos->second);
  }
}

void OptolinkSelect::datapoint_value_changed(uint8_t value) {
  std::string key = std::to_string(value);
  datapoint_value_changed(key);
}

void OptolinkSelect::datapoint_value_changed(uint16_t value) {
  std::string key = std::to_string(value);
  datapoint_value_changed(key);
}

void OptolinkSelect::datapoint_value_changed(uint32_t value) {
  std::string key = std::to_string(value);
  datapoint_value_changed(key);
}

void OptolinkSelect::datapoint_value_changed(float value) {
  std::string key = std::to_string(value);
  datapoint_value_changed(key);
}

}  // namespace optolink
}  // namespace esphome

#endif
