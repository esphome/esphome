#include "co2_setpoint.h"

#include <cmath>

#include "esphome/core/log.h"

namespace esphome::duco {

ESPHOME_LOG_TAG(TAG, "duco.co2_setpoint");

static constexpr uint8_t FUNCTION_NODE_SETTING = 0x18;
static constexpr uint8_t CO2_SETPOINT_ITEM = 0x03;

void DucoCo2Setpoint::update() {
  this->parent_->send(this, FUNCTION_NODE_SETTING, {0x00, this->address_, CO2_SETPOINT_ITEM});
}

void DucoCo2Setpoint::receive_response(const DucoMessage &message) {
  if (message.size < 3)
    return;

  const uint16_t value = message.data[1] | (static_cast<uint16_t>(message.data[2]) << 8);
  if (value <= 2000)
    this->publish_state(value);
}

void DucoCo2Setpoint::control(float value) {
  if (!this->parent_->allows_dangerous_controls()) {
    ESP_LOGW(TAG, "CO2 setpoint change rejected: allow_dangerous_controls is disabled");
    this->publish_state(this->state);
    return;
  }

  const auto setpoint = static_cast<uint16_t>(lroundf(value));
  this->parent_->send(nullptr, FUNCTION_NODE_SETTING,
                      {0x01, this->address_, CO2_SETPOINT_ITEM, static_cast<uint8_t>(setpoint >> 8),
                       static_cast<uint8_t>(setpoint)});
  // Update state immediately
  this->publish_state(value);
}

}  // namespace esphome::duco
