#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include "mbus-protocol-handler.h"
#include "mbus-frame-meta.h"
#include "mbus-frame-factory.h"

namespace esphome {
namespace mbus {

static const char *const TAG = "mbus-protocol";

void MBusProtocolHandler::register_command(MBusFrame &command,
                                           void (*response_handler)(MBusCommand *command, const MBusFrame &response),
                                           uint8_t data, uint32_t delay, bool wait_for_response) {
  MBusCommand *cmd = new MBusCommand(command, response_handler, data, this, delay, wait_for_response);
  this->_commands.push_back(cmd);
}

void MBusProtocolHandler::loop() {
  auto now = millis();
  ESP_LOGVV(TAG, "loop. _waiting_for_response = %d, _timestamp = %d, now = %d, timed out: %d",
            this->_waiting_for_response, this->_timestamp, now, (now - this->_timestamp) > this->rx_timeout);

  if (!this->_waiting_for_response && this->_commands.size()) {
    auto cmd = this->_commands.front();
    if ((now - cmd->created) < cmd->delay) {
      return;
    }

    auto frame = cmd->command;
    frame->dump();
    this->send(*frame);

    this->_waiting_for_response = true;
    this->_timestamp = now;

    delay(10);
    return;
  }

  if (this->_waiting_for_response) {
    auto command = this->_commands.front();

    if (!command->wait_for_response) {
      delay(25);

      if (command->response_handler != nullptr) {
        auto frame = MBusFrameFactory::create_empty_frame();
        command->response_handler(command, *frame);
      }

      delete_first_command();
      return;
    }

    if ((now - this->_timestamp) > this->rx_timeout) {
      ESP_LOGW(TAG, "M-Bus data: Timeout");

      if (command->response_handler != nullptr) {
        auto frame = MBusFrameFactory::create_empty_frame();
        command->response_handler(command, *frame);
      }

      delete_first_command();
      return;  // rx timeout
    }

    auto rx_status = this->receive();

    // Stop Bit received
    if (rx_status == 1) {
      if (command->response_handler != nullptr) {
        auto frame = this->parse_response();
        frame->dump();
        command->response_handler(command, *frame);
      }

      delete_first_command();
      return;
    }

    // Partial Data received
    if (rx_status == 0) {
      this->_timestamp = now;
      delay(10);
      return;
    }

    delay(25);
  }
}

void MBusProtocolHandler::delete_first_command() {
  auto command = this->_commands.front();

  delete command;
  this->_commands.pop_front();

  this->_waiting_for_response = false;
  this->_timestamp = 0;
  this->_rx_buffer.clear();
}

int8_t MBusProtocolHandler::send(MBusFrame &frame) {
  this->_rx_buffer.clear();

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

  ESP_LOGV(TAG, "Send mbus data: %s", format_hex_pretty(payload).c_str());
  this->_networkAdapter->send(payload);
  this->_timestamp = millis();

  payload.clear();

  return 0;
}

/// @return 0 if more data needed, 1 if frame is completed, -1 if response timed out
int8_t MBusProtocolHandler::receive() {
  auto rx_status = this->_networkAdapter->receive(this->_rx_buffer);

  if (rx_status == 0) {
    return 0;
  }

  if (rx_status == -1) {
    // no data received. Try next loop.
    return -1;
  }

  if (rx_status == 1) {
    // End of Frame received.
    ESP_LOGV(TAG, "Received mbus data: %s", format_hex_pretty(this->_rx_buffer).c_str());
    return 1;
  }

  return 0;
}

std::unique_ptr<MBusFrame> MBusProtocolHandler::parse_response() {
  if (this->_rx_buffer.size() <= 0) {
    return nullptr;
  }

  //     Single Character
  //    ------------------
  // 0  |      E5h       |
  //    ------------------
  if (this->_rx_buffer.at(0) == MBusFrameDefinition::ACK_FRAME.start_bit) {
    return MBusFrameFactory::create_ack_frame();
  }

  //      Short Frame
  //   ------------------
  // 0 |   Start 10h    |
  //   ------------------
  // 1 |    C Field     |
  //   ------------------
  // 2 |    A Field     |
  //   ------------------
  // 3 |   Check Sum    |
  //   ------------------
  // 4 |    Stop 16h    |
  //   ------------------
  if (this->_rx_buffer.at(0) == MBusFrameDefinition::SHORT_FRAME.start_bit &&
      this->_rx_buffer.size() == MBusFrameDefinition::SHORT_FRAME.base_frame_size) {
    return MBusFrameFactory::create_short_frame(this->_rx_buffer.at(1), this->_rx_buffer.at(2), this->_rx_buffer.at(3));
  }

  //     Control Frame
  //   ------------------
  // 0 |   Start 68h    |
  //   ------------------
  // 1 |  L Field = 3   |
  //   ------------------
  // 2 |  L Field = 3   |
  //   ------------------
  // 3 |   Start 68h    |
  //   ------------------
  // 4 |    C Field     |
  //   ------------------
  // 5 |    A Field     |
  //   ------------------
  // 6 |    CI Field    |
  //   ------------------
  // 7 |   Check Sum    |
  //   ------------------
  // 8 |    Stop 16h    |
  //   ------------------
  if (this->_rx_buffer.at(0) == MBusFrameDefinition::CONTROL_FRAME.start_bit &&
      this->_rx_buffer.size() == MBusFrameDefinition::CONTROL_FRAME.base_frame_size) {
    return MBusFrameFactory::create_control_frame(this->_rx_buffer.at(4), this->_rx_buffer.at(5),
                                                  this->_rx_buffer.at(6), this->_rx_buffer.at(7));
  }

  //     Long Frame
  //   ------------------
  // 0 |   Start 68h    |
  //   ------------------
  // 1 |    L Field     |
  //   ------------------
  // 2 |    L Field     |
  //   ------------------
  // 3 |   Start 68h    |
  //   ------------------
  // 4 |    C Field     |
  //   ------------------
  // 5 |    A Field     |
  //   ------------------
  // 6 |    CI Field    |
  //   ------------------
  // 7 |    User Data   |
  //   |  (0-252 Byte)  |
  //   ------------------
  // ..|   Check Sum    |
  //   ------------------
  // ..|    Stop 16h    |
  //   ------------------
  if (this->_rx_buffer.at(0) == MBusFrameDefinition::LONG_FRAME.start_bit) {
    std::vector<uint8_t> data(this->_rx_buffer.begin() + 7, this->_rx_buffer.end() - 2);
    auto frame =
        MBusFrameFactory::create_long_frame(this->_rx_buffer.at(4), this->_rx_buffer.at(5), this->_rx_buffer.at(6),
                                            data, this->_rx_buffer.at(this->_rx_buffer.size() - 2));
    if (frame->control_information == MBusControlInformationCodes::VARIABLE_DATA_RESPONSE_MODE1) {
      frame->variable_data = parse_variable_data_response(frame->data);
    }

    return frame;
  }
  return nullptr;
}

std::unique_ptr<MBusDataVariable> MBusProtocolHandler::parse_variable_data_response(std::vector<uint8_t> data) {
  auto response = new MBusDataVariable();
  if (data.size() < 12) {
    ESP_LOGE(TAG, "Variable Data Header less than 12 byte: %d", data.size());
    return nullptr;
  }

  // Ident.Nr.   Manufr. Version Medium Access No. Status  Signature
  // 4 Byte BCD  2 Byte  1 Byte  1 Byte   1 Byte   1 Byte  2 Byte
  //   0..3       4..5      6       7        8       9     10.. 11
  auto header = &response->header;
  std::vector<uint8_t> id(data.begin(), data.begin() + 4);
  header->id = decode_bcd_hex(id);
  header->manufacturer = decode_manufacturer(data[4], data[5]);
  header->version = data[6];
  header->medium = data[7];
  header->access_no = data[8];
  header->status = data[9];
  header->signature[0] = data[10];
  header->signature[1] = data[11];

  std::unique_ptr<MBusDataVariable> response_ptr(response);
  return response_ptr;
}

uint64_t MBusProtocolHandler::decode_bcd_hex(std::vector<uint8_t> &bcd_data) {
  uint64_t val = 0;
  size_t i;

  if (bcd_data.size() == 0) {
    return -1;
  }
  for (i = bcd_data.size(); i > 0; i--) {
    val = (val << 8) | bcd_data[i - 1];
  }

  return val;
}

std::string MBusProtocolHandler::decode_manufacturer(uint8_t byte1, uint8_t byte2) {
  int16_t m_id;
  std::vector<uint8_t> m_bytes{byte1, byte2};
  decode_int(m_bytes, &m_id);

  uint8_t m_str[4];
  m_str[0] = (char) (((m_id >> 10) & 0x001F) + 64);
  m_str[1] = (char) (((m_id >> 5) & 0x001F) + 64);
  m_str[2] = (char) (((m_id) &0x001F) + 64);
  m_str[3] = 0;

  std::string manufacturer((char *) m_str);
  return manufacturer;
}

uint8_t MBusProtocolHandler::decode_int(std::vector<uint8_t> &data, int16_t *value) {
  size_t i;
  int neg;
  *value = 0;

  auto data_size = data.size();
  if (data_size < 1) {
    return -1;
  }

  neg = data[data_size - 1] & 0x80;

  for (i = data_size; i > 0; i--) {
    if (neg) {
      *value = (*value << 8) + (data[i - 1] ^ 0xFF);
    } else {
      *value = (*value << 8) + data[i - 1];
    }
  }

  if (neg) {
    *value = *value * -1 - 1;
  }

  return 0;
}

}  // namespace mbus
}  // namespace esphome
