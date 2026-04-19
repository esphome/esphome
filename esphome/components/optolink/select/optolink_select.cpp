#ifdef USE_ARDUINO

#include "esphome/core/helpers.h"
#include "optolink_select.h"
#include "../optolink.h"

namespace esphome {
namespace optolink {

static const char *const TAG = "optolink.select";

void OptolinkSelect::set_map(std::map<std::string, std::string> *mapping) {
  mapping_ = mapping;
  FixedVector<const char *> values;
  values.init(mapping->size());

  for (auto &it : *mapping) {
    values.push_back(it.second.c_str());
  }
  traits.set_options(values);
};

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
  char key[8];
  snprintf(key, sizeof(key), "%u", value);
  datapoint_value_changed(std::string(key));
}

void OptolinkSelect::datapoint_value_changed(uint16_t value) {
  char key[16];
  snprintf(key, sizeof(key), "%u", value);
  datapoint_value_changed(std::string(key));
}

void OptolinkSelect::datapoint_value_changed(uint32_t value) {
  char key[16];
  snprintf(key, sizeof(key), "%lu", static_cast<unsigned long>(value));
  datapoint_value_changed(std::string(key));
}

void OptolinkSelect::datapoint_value_changed(float value) {
  char key[32];
  snprintf(key, sizeof(key), "%g", value);
  datapoint_value_changed(std::string(key));
}

}  // namespace optolink
}  // namespace esphome

#endif
