#include "caravan_device.h"

#ifdef USE_ESP32
namespace esphome {
namespace fendt_caravan {

static const char *const TAG = "raw.FC";

void CaravanDevice::decode(const std::string &name, const std::string &value) {
  auto *variable = this->get_variable_(name);
  if (variable)
    variable->decode(value);
};
void CaravanDevice::update() {
  ESP_LOGD(TAG, "Update called");
  if (!this->log_variables_)
    return;
  ESP_LOGI(TAG, "Variable Count :%d", this->variables_.size());
  for (auto *var : this->variables_) {
    if (var->is_active()) {
      ESP_LOGI(TAG, "Variable: %s, raw value: %s", var->get_name().c_str(), var->get_raw_value().c_str());
    }
  }
  this->log_variables_ = false;
}

}  // namespace fendt_caravan
}  // namespace esphome

#endif  // USE_ESP32
