#include "setpoint.h"

#include <cmath>

#include "esphome/core/log.h"

namespace esphome::duco {

ESPHOME_LOG_TAG(TAG, "duco.setpoint");

static constexpr uint8_t FUNCTION_NODE_SETTING = 0x18;
void DucoSetpoint::update() { this->parent_->send(this, FUNCTION_NODE_SETTING, {0x00, this->address_, this->item_}); }

void DucoSetpoint::receive_response(const DucoMessage &message) {
  if (message.size < 3)
    return;
  const uint16_t value = encode_uint16(message.data[2], message.data[1]);
  if (value >= this->traits.get_min_value() && value <= this->traits.get_max_value())
    this->publish_state(value);
}

void DucoSetpoint::control(float value) {
  if (!this->parent_->allows_system_configuration()) {
    ESP_LOGW(TAG, "Setpoint change rejected: system configuration is locked");
    if (this->has_state())
      this->publish_state(this->state);
    return;
  }
  const auto setpoint = static_cast<uint16_t>(lroundf(value));
  this->parent_->send(
      nullptr, FUNCTION_NODE_SETTING,
      {0x01, this->address_, this->item_, static_cast<uint8_t>(setpoint >> 8), static_cast<uint8_t>(setpoint)});
  // Update state immediately
  this->publish_state(value);
}

}  // namespace esphome::duco
