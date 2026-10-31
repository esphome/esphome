#include "passive_cooling.h"

#include "esphome/core/log.h"

namespace esphome::duco {

ESPHOME_LOG_TAG(TAG, "duco.passive_cooling");

static constexpr uint8_t PASSIVE_COOLING_ADDRESS = 0x01;
static constexpr uint8_t PASSIVE_COOLING_PARAMETER = 0x0a;
static constexpr uint8_t PASSIVE_COOLING_GROUP = 0x01;
static constexpr uint8_t PASSIVE_COOLING_CODES[] = {0x00, 0x01, 0x02};

void DucoPassiveCooling::update() {
  this->parent_->send(this, FUNCTION_PARAMETER, {0x00, PASSIVE_COOLING_PARAMETER, PASSIVE_COOLING_GROUP});
}

void DucoPassiveCooling::receive_response(const DucoMessage &message) {
  if (message.size < 4)
    return;

  for (size_t index = 0; index < sizeof(PASSIVE_COOLING_CODES); index++) {
    if (message.data[3] == PASSIVE_COOLING_CODES[index]) {
      this->publish_state(index);
      return;
    }
  }
  ESP_LOGV(TAG, "Unknown passive cooling mode %02X", message.data[3]);
}

void DucoPassiveCooling::control(size_t index) {
  if (!this->parent_->allows_dangerous_controls()) {
    ESP_LOGW(TAG, "Passive cooling change rejected: allow_dangerous_controls is disabled");
    if (this->has_state())
      this->publish_state(this->active_index_);
    return;
  }

  this->parent_->send(nullptr, FUNCTION_PARAMETER,
                      {PASSIVE_COOLING_ADDRESS, PASSIVE_COOLING_PARAMETER, PASSIVE_COOLING_GROUP,
                       PASSIVE_COOLING_CODES[index], 0x00, 0x00, 0x00});
}

}  // namespace esphome::duco
