#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include "mbus-protocol-handler.h"
#include "mbus-frame-meta.h"

namespace esphome {
namespace mbus {

static const char *const TAG = "mbus-protocol";

void MBusProtocolHandler::register_command(MBusFrame &command,
                                           void (*response_handler)(MBusCommand *command, const MBusFrame &response),
                                           uint8_t data) {
  MBusCommand *cmd = new MBusCommand(command, response_handler, data, this);
  this->_commands.push_back(cmd);
}

void MBusProtocolHandler::loop() {
  auto now = millis();
  ESP_LOGVV(TAG, "loop. _waiting_for_response = %d, _timestamp = %d, now = %d", this->_waiting_for_response,
            this->_timestamp, now);

  if (!this->_waiting_for_response && this->_commands.size() > 0) {
    auto frame = this->_commands.front()->command;
    this->send(*frame);
    this->_waiting_for_response = true;
    this->_timestamp = now;
    delay(10);
    return;
  }

  if (this->_waiting_for_response) {
    if ((now - this->_timestamp) > this->rx_timeout) {
      ESP_LOGV(TAG, "M-Bus data: Timeout");

      auto command = this->_commands.front();
      delete command;
      this->_commands.pop_front();

      this->_waiting_for_response = false;
      this->_timestamp = 0;
      this->_rx_buffer.clear();

      return;  // rx timeout
    }

    auto rx_status = this->receive();
    if (rx_status == 1) {
      auto command = this->_commands.front();

      if (command->response_handler != nullptr) {
        MBusFrame frame(MBusFrameType::MBUS_FRAME_TYPE_ACK);
        command->response_handler(command, frame);
      }

      delete command;
      this->_commands.pop_front();

      this->_waiting_for_response = false;
      this->_timestamp = 0;
      this->_rx_buffer.clear();
      return;
    }

    if (rx_status == 0) {
      this->_timestamp = now;
      delay(10);
      return;
    }

    delay(100);
  }
}

int8_t MBusProtocolHandler::send(MBusFrame &frame) {
  frame.dump();

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
      payload_size = MBusFrameDefinition::LONG_FRAME.base_frame_size + frame.data.size();
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
  auto rx_status = this->_networkAdapter->receive(this->_rx_buffer);

  if (rx_status == 0) {
    // received data -> set timestamp
    ESP_LOGVV(TAG, "Receive mbus data: Byte = %d.", this->_rx_buffer[this->_rx_buffer.size() - 1]);
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

MBusFrame *MBusProtocolHandler::parse() {}

}  // namespace mbus
}  // namespace esphome
