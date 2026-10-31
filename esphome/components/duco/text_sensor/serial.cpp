#include "serial.h"

#include <cstring>

namespace esphome::duco {

void DucoSerial::update() {
  // Address 0 selects the separate box serial; nodes use a different data block (main board is node 1).
  if (this->address_ == 0) {
    this->parent_->send(this, FUNCTION_NODE_DATA, {0x01, 0x01, 0x00, 0x1a, 0x10});
  } else {
    this->parent_->send(this, FUNCTION_NODE_DATA, {0x01, this->address_, 0x00, 0x08, 0x06});
  }
}

void DucoSerial::receive_response(const DucoMessage &message) {
  if (message.size > 2) {
    // The serial number starts at byte 2 and is padded with zeros
    const char *serial = reinterpret_cast<const char *>(message.data + 2);
    this->publish_state(serial, strnlen(serial, message.size - 2));
  }
}

}  // namespace esphome::duco
