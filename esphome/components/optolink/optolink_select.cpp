#ifdef USE_ARDUINO

#include "optolink_select.h"
#include "optolink.h"
#include "VitoWiFi.h"

namespace esphome {
namespace optolink {

void OptolinkSelect::control(const std::string &value) {
  for (auto it = mapping_->begin(); it != mapping_->end(); ++it) {
    if (it->second == value) {
      ESP_LOGI("OptolinkSelect", "control of select %s to value %s", get_sensor_name().c_str(), it->first.c_str());
      update_datapoint_(std::stof(it->first));
      publish_state(it->second);
      break;
    }
    if (it == mapping_->end()) {
      optolink_->set_error("unknown value %s of select %s", value.c_str(), get_sensor_name().c_str());
      ESP_LOGE("OptolinkSelect", "unknown value %s of select %s", value.c_str(), get_sensor_name().c_str());
    }
  }
};

void OptolinkSelect::value_changed(float state) {
  std::string key;
  if (div_ratio_ == 1) {
    key = std::to_string((int) state);
  } else {
    key = std::to_string(state);
  }
  auto pos = mapping_->find(key);
  if (pos == mapping_->end()) {
    optolink_->set_error("value %s not found in select %s", key.c_str(), get_sensor_name().c_str());
    ESP_LOGE("OptolinkSelect", "value %s not found in select %s", key.c_str(), get_sensor_name().c_str());
  } else {
    publish_state(pos->second);
  }
  //-----------------------------------------------publish_state(state);
};

}  // namespace optolink
}  // namespace esphome

#endif
