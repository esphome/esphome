#include "esphome/core/log.h"
#include "mbus_frame_meta.h"
#include "network_adapter.h"

namespace esphome {
namespace mbus {
static const char *const TAG = "mbus_serial_adapter";

int8_t SerialAdapter::send(std::vector<uint8_t> &payload) {
  this->uart_->write_array(payload);
  this->uart_->flush();
  return 0;
}

int8_t SerialAdapter::receive(std::vector<uint8_t> &payload) {
  if (this->uart_->available() <= 0) {
    return -1;
  }

  uint8_t byte = 0;
  while (this->uart_->available()) {
    this->uart_->read_byte(&byte);
    payload.push_back(byte);
    ESP_LOGV(TAG, "  <- 0x%X", byte);

    if (byte == MBusFrameDefinition::ACK_FRAME.start_bit) {
      // ACK_FRAME recevied
      return 1;
    }

    if (byte == MBusFrameDefinition::SHORT_FRAME.start_bit) {
      // start of short frame
      return 0;
    }
    if (byte == MBusFrameDefinition::SHORT_FRAME.stop_bit) {
      // end of short frame
      return 1;
    }

    if (byte == MBusFrameDefinition::CONTROL_FRAME.start_bit) {
      // start of control frame
      return 0;
    }
    if (byte == MBusFrameDefinition::CONTROL_FRAME.stop_bit) {
      // end of control frame
      return 1;
    }

    if (byte == MBusFrameDefinition::LONG_FRAME.start_bit) {
      // start of control frame
      return 0;
    }
    if (byte == MBusFrameDefinition::LONG_FRAME.stop_bit) {
      // end of control frame
      return 1;
    }
  }

  return 0;
}

}  // namespace mbus
}  // namespace esphome
