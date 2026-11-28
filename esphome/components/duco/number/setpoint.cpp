#include "setpoint.h"

#include <cmath>

namespace esphome::duco {

void DucoSetpoint::update() { this->parent_->send(this, FUNCTION_NODE_SETTING, {0x00, this->address_, this->item_}); }

void DucoSetpoint::receive_response(const DucoMessage &message) {
  if (message.size < 3) {
    // A write is acknowledged with a single 0x01: read the stored value back
    if (message.size == 1 && message.data[0] == 0x01)
      this->update();
    return;
  }
  // Setpoint responses are little-endian, while writes are big-endian.
  this->publish_state(encode_uint16(message.data[2], message.data[1]));
}

void DucoSetpoint::control(float value) {
  const auto setpoint = static_cast<uint16_t>(lroundf(value));
  this->parent_->send(
      this, FUNCTION_NODE_SETTING,
      {0x01, this->address_, this->item_, static_cast<uint8_t>(setpoint >> 8), static_cast<uint8_t>(setpoint)});
}

}  // namespace esphome::duco
