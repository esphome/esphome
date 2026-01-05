#include "select.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace alpha3 {

static const char *const TAG = "alpha3.select";

void Alpha3Select::control(const std::string &value) {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Parent not set");
    return;
  }

  ESP_LOGI(TAG, "Setting pump mode to: %s", value.c_str());

  if (value == "AutoAdapt") {
    this->parent_->set_mode_autoadapt();
  } else if (value == "Constant Pressure 1 (Min)") {
    this->parent_->set_mode_const_pressure(1);
  } else if (value == "Constant Pressure 2") {
    this->parent_->set_mode_const_pressure(2);
  } else if (value == "Constant Pressure 3 (Max)") {
    this->parent_->set_mode_const_pressure(3);
  } else if (value == "Proportional Pressure 1 (Min)") {
    this->parent_->set_mode_prop_pressure(1);
  } else if (value == "Proportional Pressure 2") {
    this->parent_->set_mode_prop_pressure(2);
  } else if (value == "Proportional Pressure 3 (Max)") {
    this->parent_->set_mode_prop_pressure(3);
  } else if (value == "Constant Frequency") {
    this->parent_->set_mode_const_freq();
  } else {
    ESP_LOGW(TAG, "Unknown mode: %s", value.c_str());
    return;
  }

  this->publish_state(value);
}

}  // namespace alpha3
}  // namespace esphome

#endif
