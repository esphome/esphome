#include "frost_heater.h"

#include "esphome/core/log.h"

namespace esphome::duco {

ESPHOME_LOG_TAG(TAG, "duco.frost_heater");

static constexpr uint8_t FROST_HEATER_ADDRESS = 0x01;
static constexpr uint8_t FROST_HEATER_PARAMETER = 0x20;
static constexpr uint8_t FROST_HEATER_GROUP = 0x0a;
static constexpr uint8_t FROST_HEATER_CODES[] = {0x01, 0x00};

void DucoFrostHeater::update() {
  this->parent_->send(this, FUNCTION_PARAMETER, {0x00, FROST_HEATER_PARAMETER, FROST_HEATER_GROUP});
}

void DucoFrostHeater::receive_response(const DucoMessage &message) {
  if (message.size < 4)
    return;

  for (size_t index = 0; index < sizeof(FROST_HEATER_CODES); index++) {
    if (message.data[3] == FROST_HEATER_CODES[index]) {
      this->publish_state(index);
      return;
    }
  }
  ESP_LOGV(TAG, "Unknown frost heater mode %02X", message.data[3]);
}

void DucoFrostHeater::control(size_t index) {
  if (!this->parent_->allows_dangerous_controls()) {
    ESP_LOGW(TAG, "Frost heater change rejected: allow_dangerous_controls is disabled");
    if (this->has_state())
      this->publish_state(this->active_index_);
    return;
  }

  this->parent_->send(nullptr, FUNCTION_PARAMETER,
                      {FROST_HEATER_ADDRESS, FROST_HEATER_PARAMETER, FROST_HEATER_GROUP, FROST_HEATER_CODES[index],
                       0x00, 0x00, 0x00});
  // Update state immediately
  this->publish_state(index);
}

}  // namespace esphome::duco
