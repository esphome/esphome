#include "manual_time.h"

#include <cmath>

#include "esphome/core/log.h"

namespace esphome::duco {

ESPHOME_LOG_TAG(TAG, "duco.manual_time");

static constexpr uint8_t FUNCTION_NODE_SETTING = 0x18;
static constexpr uint8_t MANUAL_TIME_ITEM = 0x09;

void DucoManualTime::update() {
  this->parent_->send(this, FUNCTION_NODE_SETTING, {0x00, this->address_, MANUAL_TIME_ITEM});
}

void DucoManualTime::receive_response(const DucoMessage &message) {
  if (message.size < 3)
    return;

  const uint16_t value = message.data[1] | (static_cast<uint16_t>(message.data[2]) << 8);
  if (value >= 5 && value <= 720)
    this->publish_state(value);
}

void DucoManualTime::control(float value) {
  if (!this->parent_->allows_dangerous_controls()) {
    ESP_LOGW(TAG, "Manual ventilation time change rejected: allow_dangerous_controls is disabled");
    this->publish_state(this->state);
    return;
  }

  const auto minutes = static_cast<uint16_t>(lroundf(value));
  this->parent_->send(nullptr, FUNCTION_NODE_SETTING,
                      {0x01, this->address_, MANUAL_TIME_ITEM, static_cast<uint8_t>(minutes >> 8),
                       static_cast<uint8_t>(minutes)});
  // Update state immediately
  this->publish_state(value);
}

}  // namespace esphome::duco
