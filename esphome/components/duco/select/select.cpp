#include "select.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::duco {

ESPHOME_LOG_TAG(TAG, "duco.select");

static constexpr uint8_t MODE_ACCEPTED = 0x01;

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

void DucoSelect::update() { this->parent_->send(this, FUNCTION_NODE, {0x02, 0x01}); }

bool DucoSelect::receive_response(const DucoMessage &message) {
  if (message.function != FUNCTION_NODE + 2)
    return false;
  // A mode change is answered with MODE_ACCEPTED; the next update reads the new mode
  if (message.size >= 1 && message.data[0] != MODE_ACCEPTED) {
    size_t index = 0;  // unknown codes show as AUTO
    for (size_t i = 0; i < sizeof(MODE_CODES); i++) {
      if (progmem_read_byte(&MODE_CODES[i]) == message.data[0]) {
        index = i;
        break;
      }
    }
    ESP_LOGD(TAG, "Current mode: %s", this->option_at(index));
    this->publish_state(index);
  }
  return true;
}

void DucoSelect::control(size_t index) {
  this->parent_->send(this, FUNCTION_NODE, {0x04, 0x01, progmem_read_byte(&MODE_CODES[index])});
}

}  // namespace esphome::duco
