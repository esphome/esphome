#include "serial.h"

#include <cstring>

namespace esphome::duco {

void DucoSerial::update() { this->parent_->send(this, FUNCTION_NODE_DATA, {0x01, 0x01, 0x00, 0x1a, 0x10}); }

void DucoSerial::receive_response(const DucoMessage &message) {
  if (message.size > 2) {
    // The serial number starts at byte 2 and is padded with zeros
    const char *serial = reinterpret_cast<const char *>(message.data + 2);
    this->publish_state(serial, strnlen(serial, message.size - 2));
  }
}

}  // namespace esphome::duco
