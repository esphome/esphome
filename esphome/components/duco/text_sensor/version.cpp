#include "version.h"

#include <cstdio>

namespace esphome::duco {

void DucoVersion::update() {
  this->parent_->send(this, FUNCTION_NODE_DATA, {0x01, this->address_, 0x00, 0x00, 0x04});
}

void DucoVersion::receive_response(const DucoMessage &message) {
  if (message.size < 7)
    return;

  const uint16_t major = encode_uint16(message.data[3], message.data[2]);
  char version[20];
  snprintf(version, sizeof(version), "%u.%u.%u.%u", static_cast<unsigned>(major),
           static_cast<unsigned>(message.data[4]), static_cast<unsigned>(message.data[6]),
           static_cast<unsigned>(message.data[5]));
  this->publish_state(version);
}

}  // namespace esphome::duco
