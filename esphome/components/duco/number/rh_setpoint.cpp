#include "rh_setpoint.h"

#include <cmath>

#include "esphome/core/log.h"

namespace esphome::duco {

ESPHOME_LOG_TAG(TAG, "duco.rh_setpoint");

static constexpr uint8_t FUNCTION_NODE_SETTING = 0x18;
static constexpr uint8_t RH_SETPOINT_ITEM = 0x04;

void DucoRhSetpoint::update() {
  this->parent_->send(this, FUNCTION_NODE_SETTING, {0x00, this->address_, RH_SETPOINT_ITEM});
}

void DucoRhSetpoint::receive_response(const DucoMessage &message) {
  if (message.size < 3)
    return;

  const uint16_t value = message.data[1] | (static_cast<uint16_t>(message.data[2]) << 8);
  if (value <= 90)
    this->publish_state(value);
}

void DucoRhSetpoint::control(float value) {
  if (!this->parent_->allows_dangerous_controls()) {
    ESP_LOGW(TAG, "RH setpoint change rejected: allow_dangerous_controls is disabled");
    this->publish_state(this->state);
    return;
  }

  const auto setpoint = static_cast<uint16_t>(lroundf(value));
  this->parent_->send(nullptr, FUNCTION_NODE_SETTING,
                      {0x01, this->address_, RH_SETPOINT_ITEM, static_cast<uint8_t>(setpoint >> 8),
                       static_cast<uint8_t>(setpoint)});
  // Update state immediately
  this->publish_state(value);
}

}  // namespace esphome::duco
