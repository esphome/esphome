#include "uptime.h"

#include <cstdio>

namespace esphome::duco {

void DucoUptime::update() {
  this->parent_->send(this, FUNCTION_NODE_DATA, {0x01, this->address_, 0x00, 0x49, 0x04});
}

void DucoUptime::receive_response(const DucoMessage &message) {
  if (message.size < 10)
    return;

  const uint16_t uptime_minutes = encode_uint16(message.data[9], message.data[8]);
  const uint16_t days = uptime_minutes / 1440;
  const uint8_t hours = (uptime_minutes % 1440) / 60;
  const uint8_t minutes = uptime_minutes % 60;
  char formatted[24];
  snprintf(formatted, sizeof(formatted), "%ud %uh %um", static_cast<unsigned>(days),
           static_cast<unsigned>(hours), static_cast<unsigned>(minutes));
  this->publish_state(formatted);
}

}  // namespace esphome::duco
