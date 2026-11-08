#include "rh_delta_mode.h"

#include "esphome/core/log.h"

namespace esphome::duco {

ESPHOME_LOG_TAG(TAG, "duco.rh_delta_mode");

static constexpr uint8_t FUNCTION_NODE_SETTING = 0x18;
static constexpr uint8_t RH_DELTA_MODE_ITEM = 0x05;

void DucoRhDeltaMode::update() {
  this->parent_->send(this, FUNCTION_NODE_SETTING, {0x00, this->address_, RH_DELTA_MODE_ITEM});
}

void DucoRhDeltaMode::receive_response(const DucoMessage &message) {
  if (message.size < 2 || message.data[1] > 1)
    return;

  this->publish_state(message.data[1]);
}

void DucoRhDeltaMode::control(size_t index) {
  if (!this->parent_->allows_dangerous_controls()) {
    ESP_LOGW(TAG, "RH delta mode change rejected: allow_dangerous_controls is disabled");
    if (this->has_state())
      this->publish_state(this->active_index_);
    return;
  }

  this->parent_->send(nullptr, FUNCTION_NODE_SETTING,
                      {0x01, this->address_, RH_DELTA_MODE_ITEM, 0x00, static_cast<uint8_t>(index)});
  // Update state immediately
  this->publish_state(index);
}

}  // namespace esphome::duco
