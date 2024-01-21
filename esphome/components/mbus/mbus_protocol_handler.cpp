#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include "mbus_frame_factory.h"
#include "mbus_frame_meta.h"
#include "mbus_protocol_handler.h"

namespace esphome {
namespace mbus {

static const char *const TAG = "mbus_protocol";

void MBusProtocolHandler::register_command(MBusFrame &command,
                                           void (*response_handler)(MBusCommand *command, const MBusFrame &response),
                                           uint8_t step, uint32_t delay, bool wait_for_response) {
  MBusCommand *cmd = new MBusCommand(command, response_handler, step, this->mbus_, delay, wait_for_response);
  this->commands_.push_back(cmd);
}

void MBusProtocolHandler::loop() {
  auto now = millis();
  ESP_LOGVV(TAG, "loop. waiting_for_response_ = %d, timestamp_ = %d, now = %d, timed out: %d",
            this->waiting_for_response_, this->timestamp_, now,
            (now - this->timestamp_) > MBusProtocolHandler::RX_TIMEOUT);

  if (!this->waiting_for_response_ && !this->commands_.empty()) {
    auto *cmd = this->commands_.front();
    if ((now - cmd->created) < cmd->delay) {
      return;
    }

    auto *frame = cmd->command;
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    frame->dump();
#endif
    this->send_(*frame);

    this->waiting_for_response_ = true;
    this->timestamp_ = now;

    delay(10);
    return;
  }

  if (this->waiting_for_response_) {
    auto *command = this->commands_.front();

    if (!command->wait_for_response) {
      delay(25);

      if (command->response_handler != nullptr) {
        auto frame = MBusFrameFactory::create_empty_frame();
        command->response_handler(command, *frame);
      }

      delete_first_command_();
      return;
    }

    if ((now - this->timestamp_) > MBusProtocolHandler::RX_TIMEOUT) {
      ESP_LOGW(TAG, "M-Bus data: Timeout");

      if (command->response_handler != nullptr) {
        auto frame = MBusFrameFactory::create_empty_frame();
        command->response_handler(command, *frame);
      }

      delete_first_command_();
      return;  // rx timeout
    }

    auto rx_status = this->receive_();

    // Stop Bit received
    if (rx_status == 1) {
      if (command->response_handler != nullptr) {
        auto frame = this->parse_response_();
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
        frame->dump();
#endif
        command->response_handler(command, *frame);
      }

      delete_first_command_();
      return;
    }

    // Partial Data received
    if (rx_status == 0) {
      this->timestamp_ = now;
      delay(10);
      return;
    }

    delay(25);
  }
}

void MBusProtocolHandler::delete_first_command_() {
  auto *command = this->commands_.front();

  delete command;
  this->commands_.pop_front();

  this->waiting_for_response_ = false;
  this->timestamp_ = 0;
  this->rx_buffer_.clear();
}

int8_t MBusProtocolHandler::send_(MBusFrame &frame) {
  this->rx_buffer_.clear();

  uint8_t payload_size = 0;
  switch (frame.frame_type) {
    case MBUS_FRAME_TYPE_EMPTY:
      payload_size = 0;
      break;
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
  this->network_adapter_->send(payload);
  this->timestamp_ = millis();

  payload.clear();

  return 0;
}

/// @return 0 if more data needed, 1 if frame is completed, -1 if response timed out
int8_t MBusProtocolHandler::receive_() {
  auto rx_status = this->network_adapter_->receive(this->rx_buffer_);

  if (rx_status == 0) {
    return 0;
  }

  if (rx_status == -1) {
    // no data received. Try next loop.
    return -1;
  }

  if (rx_status == 1) {
    // End of Frame received.
    ESP_LOGV(TAG, "Received mbus data: %s", format_hex_pretty(this->rx_buffer_).c_str());
    return 1;
  }

  return 0;
}

std::unique_ptr<MBusFrame> MBusProtocolHandler::parse_response_() {
  if (this->rx_buffer_.empty()) {
    return nullptr;
  }

  //     Single Character
  //    ------------------
  // 0  |      E5h       |
  //    ------------------
  if (this->rx_buffer_.at(0) == MBusFrameDefinition::ACK_FRAME.start_bit) {
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
  if (this->rx_buffer_.at(0) == MBusFrameDefinition::SHORT_FRAME.start_bit &&
      this->rx_buffer_.size() == MBusFrameDefinition::SHORT_FRAME.base_frame_size) {
    return MBusFrameFactory::create_short_frame(this->rx_buffer_.at(1), this->rx_buffer_.at(2), this->rx_buffer_.at(3));
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
  if (this->rx_buffer_.at(0) == MBusFrameDefinition::CONTROL_FRAME.start_bit &&
      this->rx_buffer_.size() == MBusFrameDefinition::CONTROL_FRAME.base_frame_size) {
    return MBusFrameFactory::create_control_frame(this->rx_buffer_.at(4), this->rx_buffer_.at(5),
                                                  this->rx_buffer_.at(6), this->rx_buffer_.at(7));
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
  if (this->rx_buffer_.at(0) == MBusFrameDefinition::LONG_FRAME.start_bit) {
    std::vector<uint8_t> data(this->rx_buffer_.begin() + 7, this->rx_buffer_.end() - 2);
    auto frame =
        MBusFrameFactory::create_long_frame(this->rx_buffer_.at(4), this->rx_buffer_.at(5), this->rx_buffer_.at(6),
                                            data, this->rx_buffer_.at(this->rx_buffer_.size() - 2));
    if (frame->control_information == MBusControlInformationCodes::VARIABLE_DATA_RESPONSE_MODE1) {
      frame->variable_data = parse_variable_data_response_(frame->data);
    }

    return frame;
  }
  return nullptr;
}

// variable data response
// ------------------------------------------------------------------------------------
// | Fixed Data Header | Variable Data Blocks (Records) |  MDH   |  Mfg.specific data |
// |     12 Byte       |        variable number         | 1 Byte |    variable number |
// ------------------------------------------------------------------------------------
std::unique_ptr<MBusDataVariable> MBusProtocolHandler::parse_variable_data_response_(std::vector<uint8_t> data) {
  auto response = make_unique<MBusDataVariable>();
  if (data.size() < 12) {
    ESP_LOGE(TAG, "Variable Data Header less than 12 byte: %d", data.size());
    return nullptr;
  }

  // parse header
  // -----------------------------------------------------------------------------
  // | Ident.Nr.  | Manufr. | Version | Medium | Access No. | Status | Signature |
  // | 4 Byte BCD | 2 Byte  | 1 Byte  | 1 Byte | 1 Byte     | 1 Byte | 2 Byte    |
  // |    0..3    |  4..5   |    6    |   7    |     8      |    9   |  10.. 11  |
  // -----------------------------------------------------------------------------
  auto *header = &response->header;
  std::vector<uint8_t> id(data.begin(), data.begin() + 4);
  header->id[0] = data[0];
  header->id[1] = data[1];
  header->id[2] = data[2];
  header->id[3] = data[3];
  header->manufacturer[0] = data[4];
  header->manufacturer[1] = data[5];
  header->version = data[6];
  header->medium = data[7];
  header->access_no = data[8];
  header->status = data[9];
  header->signature[0] = data[10];
  header->signature[1] = data[11];

  // parse records
  // -------------------------------------------------------------------------
  // |   DIF   |        DIFE        |   VIF  |        VIFE        |   Data   |
  // | 1 Byte  | 0-10 (1 Byte each) | 1 Byte | 0-10 (1 Byte each) | 0-N Byte |
  // | Data Information Block DIB   | Value Information Block VIB |
  // |                 Data Record Header DRH                     |
  // --------------------------------------------------------------

  auto it = data.begin() + 12;
  while (it < data.end()) {
    if ((*it & 0xFF) == MBusDataDifMask::IDLE_FILLER) {
      it++;
      continue;
    }

    // The manufacturer data header (MDH) is made up by the character 0Fh or 1Fh
    // and indicates the beginning of the manufacturer specific part of the user data
    // and should be omitted, if there is no manufacturer specific data
    if ((*it & 0xFF) == MBusDataDifMask::MANUFACTURER_SPECIFIC ||
        (*it & 0xFF) == MBusDataDifMask::MORE_RECORDS_FOLLOW) {
      it++;
      continue;
    }

    MBusDataRecord record;

    // DIF
    //    Bit 7         6         5          4       3     2      1     0
    // ----------------------------------------------------------------------
    // | Extension | LSB of  |   Function Field  |   Data Field:            |
    // |    Bit    | storage |                   | Lengh and coding of data |
    // |           | Number  |                   |                          |
    // ----------------------------------------------------------------------

    record.drh.dib.dif = *it;
    // Extension Bit of DIF / DIFE Frame set => next Frame is DIFE
    while (it < data.end() && (*it & MBusDataDifMask::EXTENSION_BIT)) {
      it++;
      record.drh.dib.dife.push_back(*it);
    }
    it++;

    // VIB
    record.drh.vib.vif = *it;

    // Extension Bit of VIF / VIFE Frame set => next Frame is VIFE
    while (it < data.end() && (*it & MBusDataVifMask::EXTENSION_BIT)) {
      it++;
      record.drh.vib.vife.push_back(*it);
    }
    it++;

    auto data_len = get_dif_datalength_(record.drh.dib.dif, it);
    record.data.insert(record.data.begin(), it, it + data_len);
    it = it + data_len;

    response->records.push_back(record);
  }

  return response;
}

int8_t MBusProtocolHandler::get_dif_datalength_(uint8_t dif, std::vector<uint8_t>::iterator &it) {
  static const uint8_t DIF_DATA_LENGTH_MASK = 0x0F;
  switch (dif & DIF_DATA_LENGTH_MASK) {
    case 0x0:
      return 0;
    case 0x1:
      return 1;
    case 0x2:
      return 2;
    case 0x3:
      return 3;
    case 0x4:
      return 4;
    case 0x5:
      return 5;
    case 0x6:
      return 6;
    case 0x7:
      return 8;
    case 0x8:
      return 0;
    case 0x9:
      return 1;
    case 0xA:
      return 2;
    case 0xB:
      return 3;
    case 0xC:
      return 4;
    case 0xD: {
      // variable data length,
      // data length stored in data field
      uint8_t data_1 = *it;
      it++;

      if (data_1 <= 0xBF) {
        // ASCII string with LVAR characters
        return data_1;
      } else if (data_1 >= 0xC0 && data_1 <= 0xCF) {
        // positive BCD number with (LVAR - C0h) • 2 digits
        return (data_1 - 0xC0) * 2;
      } else if (data_1 >= 0xD0 && data_1 <= 0xDF) {
        // negative BCD number with (LVAR - D0h) • 2 digits
        return (data_1 - 0xD0) * 2;
      } else if (data_1 >= 0xE0 && data_1 <= 0xEF) {
        // binary number with (LVAR - E0h) bytes
        return data_1 - 0xE0;
      } else if (data_1 >= 0xF0 && data_1 <= 0xFA) {
        // floating point number with (LVAR - F0h) bytes [to be defined]
        return data_1 - 0xF0;
      }

      ESP_LOGE(TAG, "get_dif_datalength(): invalid mask = %d", data_1);
      return 0;
    }
    case 0xE:
      return 6;
    case 0xF:
      return 8;
    default:  // never reached
      ESP_LOGE(TAG, "Invalid value for diff data length = %d", dif & DIF_DATA_LENGTH_MASK);
      return 0x0;
  }
}

}  // namespace mbus
}  // namespace esphome
