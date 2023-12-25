#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include "mbus-protocol-handler.h"
#include "mbus-frame-meta.h"

namespace esphome {
namespace mbus {
static const char *const TAG = "mbus-protocol";

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

  return 0;
}

bool MBusProtocolHandler::is_ack_resonse() {
  return this->_rx_buffer.size() == 1 && this->_rx_buffer[0] == MBusFrameDefinition::ACK_FRAME.start_bit;
}

int8_t MBusProtocolHandler::send(MBusFrame &frame) {
  uint8_t payload_size = 0;
  switch (frame.frame_type) {
    case MBUS_FRAME_TYPE_ACK:
      payload_size = MBusFrameDefinition::ACK_FRAME.base_frame_size;
      break;
    case MBUS_FRAME_TYPE_SHORT:
      payload_size = MBusFrameDefinition::SHORT_FRAME.base_frame_size;
      break;
    case MBUS_FRAME_TYPE_CONTROL:
      payload_size = MBusFrameDefinition::CONTROL_FRAME.base_frame_size;
      break;
    case MBUS_FRAME_TYPE_LONG:
      payload_size = MBusFrameDefinition::LONG_FRAME.base_frame_size;
      break;
  }

  std::vector<uint8_t> payload(payload_size, 0);
  MBusFrame::serialize(frame, payload);

  ESP_LOGD(TAG, "Send mbus data: %s", format_hex_pretty(payload).c_str());
  this->_networkAdapter->send(payload);
  this->_timestamp = millis();

  payload.clear();

  return 0;
}

/// @return 0 if more data needed, 1 if frame is completed, -1 if response timed out
int8_t MBusProtocolHandler::receive() {
  auto now = millis();
  if (now - this->_timestamp > this->rx_timeout) {
    ESP_LOGV(TAG, "Receive mbus data: Timeout");
    this->_rx_buffer.clear();
    this->_timestamp = 0;
    return -1;  // rx timeout
  }

  auto rx_status = this->_networkAdapter->receive(this->_rx_buffer);

  if (rx_status == 0) {
    // received data -> set timestamp
    ESP_LOGVV(TAG, "Receive mbus data: Byte = %d.", this->_rx_buffer[this->_rx_buffer.size() - 1]);
    this->_timestamp = now;
    return 0;
  }

  if (rx_status == -1) {
    // no data received. Try next loop.
    return 0;
  }

  if (rx_status == 1) {
    // End of Frame received.
    ESP_LOGD(TAG, "Received mbus data: %s", format_hex_pretty(this->_rx_buffer).c_str());
    return 1;
  }

  return 0;
}

}  // namespace mbus
}  // namespace esphome
