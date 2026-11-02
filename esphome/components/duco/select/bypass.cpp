#include "bypass.h"

#include "esphome/core/log.h"

namespace esphome::duco {

ESPHOME_LOG_TAG(TAG, "duco.bypass");

static constexpr uint8_t BYPASS_ADDRESS = 0x01;
static constexpr uint8_t BYPASS_GROUP = 0x0a;
static constexpr uint8_t BYPASS_PARAMETER = 0x10;
static constexpr uint8_t ADAPTIVE_BYPASS_PARAMETER = 0x11;
static constexpr uint8_t BYPASS_CODES[] = {0x00, 0x02, 0x01};
static constexpr uint8_t ADAPTIVE_BYPASS_CODES[] = {0x01, 0x00};

void DucoBypassControl::update() {
  this->parent_->send(this, FUNCTION_PARAMETER, {0x00, BYPASS_PARAMETER, BYPASS_GROUP});
}

void DucoBypassControl::receive_response(const DucoMessage &message) {
  if (message.size < 4)
    return;

  for (size_t index = 0; index < sizeof(BYPASS_CODES); index++) {
    if (message.data[3] == BYPASS_CODES[index]) {
      this->publish_state(index);
      return;
    }
  }
  ESP_LOGV(TAG, "Unknown bypass mode %02X", message.data[3]);
}

void DucoBypassControl::control(size_t index) {
  if (!this->parent_->allows_dangerous_controls()) {
    ESP_LOGW(TAG, "Bypass change rejected: allow_dangerous_controls is disabled");
    if (this->has_state())
      this->publish_state(this->active_index_);
    return;
  }

  this->parent_->send(nullptr, FUNCTION_PARAMETER,
                      {BYPASS_ADDRESS, BYPASS_PARAMETER, BYPASS_GROUP, BYPASS_CODES[index], 0x00, 0x00, 0x00});
  // Update state immediately
  this->publish_state(index);
}

void DucoBypassAdaptiveControl::update() {
  this->parent_->send(this, FUNCTION_PARAMETER, {0x00, ADAPTIVE_BYPASS_PARAMETER, BYPASS_GROUP});
}

void DucoBypassAdaptiveControl::receive_response(const DucoMessage &message) {
  if (message.size < 4)
    return;

  for (size_t index = 0; index < sizeof(ADAPTIVE_BYPASS_CODES); index++) {
    if (message.data[3] == ADAPTIVE_BYPASS_CODES[index]) {
      this->publish_state(index);
      return;
    }
  }
  ESP_LOGV(TAG, "Unknown adaptive bypass mode %02X", message.data[3]);
}

void DucoBypassAdaptiveControl::control(size_t index) {
  if (!this->parent_->allows_dangerous_controls()) {
    ESP_LOGW(TAG, "Adaptive bypass change rejected: allow_dangerous_controls is disabled");
    if (this->has_state())
      this->publish_state(this->active_index_);
    return;
  }

  this->parent_->send(nullptr, FUNCTION_PARAMETER,
                      {BYPASS_ADDRESS, ADAPTIVE_BYPASS_PARAMETER, BYPASS_GROUP, ADAPTIVE_BYPASS_CODES[index], 0x00,
                       0x00, 0x00});
  // Update state immediately
  this->publish_state(index);
}

}  // namespace esphome::duco
