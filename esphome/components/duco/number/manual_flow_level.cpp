#include "manual_flow_level.h"

#include <cmath>

#include "esphome/core/log.h"

namespace esphome::duco {

ESPHOME_LOG_TAG(TAG, "duco.manual_flow_level");

static constexpr uint8_t FUNCTION_NODE_SETTING = 0x18;

void DucoManualFlowLevel::update() {
  this->parent_->send(this, FUNCTION_NODE_SETTING, {0x00, this->address_, this->item_});
}

void DucoManualFlowLevel::receive_response(const DucoMessage &message) {
  if (message.size < 3)
    return;

  const uint16_t value = message.data[1] | (static_cast<uint16_t>(message.data[2]) << 8);
  if (value <= 100)
    this->publish_state(value);
}

void DucoManualFlowLevel::control(float value) {
  if (!this->parent_->allows_dangerous_controls()) {
    ESP_LOGW(TAG, "Manual ventilation level change rejected: allow_dangerous_controls is disabled");
    this->publish_state(this->state);
    return;
  }

  const auto level = static_cast<uint16_t>(lroundf(value));
  this->parent_->send(nullptr, FUNCTION_NODE_SETTING,
                      {0x01, this->address_, this->item_, static_cast<uint8_t>(level >> 8),
                       static_cast<uint8_t>(level)});
  // Update state immediately
  this->publish_state(value);
}

}  // namespace esphome::duco
