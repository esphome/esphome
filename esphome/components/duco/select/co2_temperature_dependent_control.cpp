#include "co2_temperature_dependent_control.h"

#include "esphome/core/log.h"

namespace esphome::duco {

ESPHOME_LOG_TAG(TAG, "duco.co2_temperature_dependent_control");

static constexpr uint8_t FUNCTION_NODE_SETTING = 0x18;
static constexpr uint8_t TEMPERATURE_DEPENDENT_CONTROL_ITEM = 0x0E;

void DucoCo2TemperatureDependentControl::update() {
  this->parent_->send(this, FUNCTION_NODE_SETTING, {0x00, this->address_, TEMPERATURE_DEPENDENT_CONTROL_ITEM});
}

void DucoCo2TemperatureDependentControl::receive_response(const DucoMessage &message) {
  if (message.size < 2 || message.data[1] > 1)
    return;

  this->publish_state(message.data[1]);
}

void DucoCo2TemperatureDependentControl::control(size_t index) {
  if (!this->parent_->allows_dangerous_controls()) {
    ESP_LOGW(TAG, "CO2 temperature-dependent control change rejected: allow_dangerous_controls is disabled");
    if (this->has_state())
      this->publish_state(this->active_index_);
    return;
  }

  this->parent_->send(nullptr, FUNCTION_NODE_SETTING,
                      {0x01, this->address_, TEMPERATURE_DEPENDENT_CONTROL_ITEM, 0x00,
                       static_cast<uint8_t>(index)});
  // Update state immediately
  this->publish_state(index);
}

}  // namespace esphome::duco
