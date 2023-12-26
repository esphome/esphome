#include "network-adapter.h"
#include "mbus-frame-meta.h"

namespace esphome {
namespace mbus {

int8_t SerialAdapter::send(std::vector<uint8_t> &payload) {
  this->_uart->write_array(payload);
  this->_uart->flush();
  return 0;
}

int8_t SerialAdapter::receive(std::vector<uint8_t> &payload) {
  if (this->_uart->available() <= 0) {
    return -1;
  }

  uint8_t byte = 0;
  while (this->_uart->available()) {
    this->_uart->read_byte(&byte);
    payload.push_back(byte);

    if (byte == MBusFrameDefinition::ACK_FRAME.start_bit) {
      // ACK_FRAME recevied
      return 1;
    }

    if (byte == MBusFrameDefinition::SHORT_FRAME.start_bit) {
      // start of short frame
      payload.clear();
      return 0;
    }
    if (byte == MBusFrameDefinition::SHORT_FRAME.stop_bit) {
      // end of short frame
      return 1;
    }

    if (byte == MBusFrameDefinition::CONTROL_FRAME.start_bit) {
      // start of control frame
      payload.clear();
      return 0;
    }
    if (byte == MBusFrameDefinition::CONTROL_FRAME.stop_bit) {
      // end of control frame
      return 1;
    }

    if (byte == MBusFrameDefinition::LONG_FRAME.start_bit) {
      // start of control frame
      payload.clear();
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
