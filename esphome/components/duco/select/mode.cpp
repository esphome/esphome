#include "mode.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::duco {

ESPHOME_LOG_TAG(TAG, "duco.select");

// Box code of each option, in the order of DUCO_MODE_OPTIONS in __init__.py
static constexpr uint8_t MODE_CODES[] PROGMEM = {
    0x00,  // AUTO
    0x04,  // MAN1
    0x05,  // MAN2
    0x06,  // MAN3
    0x07,  // EMPT
    0x08,  // CNT1
    0x09,  // CNT2
    0x0a,  // CNT3
    0x84,  // MAN1x2
    0x85,  // MAN2x2
    0x86,  // MAN3x2
    0xc4,  // MAN1x3
    0xc5,  // MAN2x3
    0xc6,  // MAN3x3
};

void DucoSelect::update() { this->parent_->send(this, FUNCTION_NODE, {0x02, this->address_}); }

void DucoSelect::receive_response(const DucoMessage &message) {
  if (message.size < 1)
    return;
  for (size_t i = 0; i < sizeof(MODE_CODES); i++) {
    if (progmem_read_byte(&MODE_CODES[i]) == message.data[0]) {
      this->publish_state(i);
      return;
    }
  }
  ESP_LOGV(TAG, "Unknown mode %02X", message.data[0]);
}

void DucoSelect::control(size_t index) {
  this->parent_->send(nullptr, FUNCTION_NODE, {0x04, this->address_, progmem_read_byte(&MODE_CODES[index])});
  // Update state immediately
  this->publish_state(index);
}

}  // namespace esphome::duco
