#include "zigbee_proxy.h"

#ifdef USE_ZIGBEE_PROXY

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace zigbee_proxy {

static const char *const TAG = "zigbee_proxy";

// CRC-CCITT lookup table for polynomial 0x1021 (x^16 + x^12 + x^5 + 1)
static const uint16_t CRC_TABLE[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7, 0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD,
    0xE1CE, 0xF1EF, 0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6, 0x9339, 0x8318, 0xB37B, 0xA35A,
    0xD3BD, 0xC39C, 0xF3FF, 0xE3DE, 0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485, 0xA56A, 0xB54B,
    0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D, 0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC, 0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861,
    0x2802, 0x3823, 0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B, 0x5AF5, 0x4AD4, 0x7AB7, 0x6A96,
    0x1A71, 0x0A50, 0x3A33, 0x2A12, 0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A, 0x6CA6, 0x7C87,
    0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41, 0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70, 0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A,
    0x9F59, 0x8F78, 0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F, 0x1080, 0x00A1, 0x30C2, 0x20E3,
    0x5004, 0x4025, 0x7046, 0x6067, 0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E, 0x02B1, 0x1290,
    0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256, 0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405, 0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E,
    0xC71D, 0xD73C, 0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634, 0xD94C, 0xC96D, 0xF90E, 0xE92F,
    0x99C8, 0x89E9, 0xB98A, 0xA9AB, 0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3, 0xCB7D, 0xDB5C,
    0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A, 0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9, 0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83,
    0x1CE0, 0x0CC1, 0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8, 0x6E17, 0x7E36, 0x4E55, 0x5E74,
    0x2E93, 0x3EB2, 0x0ED1, 0x1EF0};

uint16_t ZigbeeProxy::calculate_crc_(const uint8_t *data, size_t length) {
  uint16_t crc = ASH_CRC_INIT;
  for (size_t i = 0; i < length; i++) {
    crc = (crc << 8) ^ CRC_TABLE[(crc >> 8) ^ data[i]];
  }
  return crc;
}

bool ZigbeeProxy::validate_frame_crc_() {
  // CRC is calculated over control byte + data
  // rx_buffer_[0] contains control byte, rx_buffer_[1..rx_buffer_index_-3] contains data
  // rx_buffer_[rx_buffer_index_-2] and rx_buffer_[rx_buffer_index_-1] contain CRC
  if (this->rx_buffer_index_ < 3) {
    // Frame too short to contain CRC
    return false;
  }

  // Calculate CRC over control + data (exclude CRC bytes)
  uint16_t calculated = this->calculate_crc_(this->rx_buffer_.data(), this->rx_buffer_index_ - 2);

  // Extract received CRC (big-endian)
  uint16_t received = (static_cast<uint16_t>(this->rx_buffer_[this->rx_buffer_index_ - 2]) << 8) |
                      this->rx_buffer_[this->rx_buffer_index_ - 1];

  if (calculated != received) {
    ESP_LOGW(TAG, "CRC validation failed: calculated=0x%04X, received=0x%04X", calculated, received);
    return false;
  }

  return true;
}

void ZigbeeProxy::parse_control_byte_(uint8_t control) {
  // Decode frame type based on bit patterns:
  // DATA:  0xxxxxxx (bit 7 = 0)
  // ACK:   10x0xxxx (bits 7-6 = 10, bit 5 = 0)
  // NAK:   10x1xxxx (bits 7-6 = 10, bit 5 = 1)
  // RST:   11000000 (0xC0)
  // RSTACK: 11000001 (0xC1)
  // ERROR: 11000010 (0xC2)

  AshFrameType frame_type;
  if ((control & 0x80) == 0) {
    // Bit 7 = 0: DATA frame
    frame_type = AshFrameType::DATA;
  } else if ((control & 0xC0) == 0x80) {
    // Bits 7-6 = 10: ACK or NAK
    // ACK format: 100nrPPP (bit 5 = 0)
    // NAK format: 101nrPPP (bit 5 = 1)
    if ((control & 0x20) == 0) {
      frame_type = AshFrameType::ACK;
    } else {
      frame_type = AshFrameType::NAK;
    }
  } else {
    // Bits 7-6 = 11: control frames (RST, RSTACK, ERROR)
    uint8_t control_bits = control & 0x07;
    if (control_bits == 0x00) {
      frame_type = AshFrameType::RST;
    } else if (control_bits == 0x01) {
      frame_type = AshFrameType::RSTACK;
    } else if (control_bits == 0x02) {
      frame_type = AshFrameType::ERROR;
    } else {
      ESP_LOGW(TAG, "Unknown control frame type: 0x%02X", control);
      return;
    }
  }

  // Extract sequence numbers from DATA frame format: 0ffrPPPP
  // Bits 6-4 = frmNum, bit 3 = reTx, bits 2-0 = ackNum
  uint8_t frame_num = (control >> 4) & 0x07;  // Bits 6-4
  uint8_t ack_num = control & 0x07;           // Bits 2-0
  bool retx = (control & 0x08) != 0;          // Bit 3 (for DATA frames)

  ESP_LOGV(TAG, "Parsed control byte: type=%d, frmNum=%d, ackNum=%d, reTx=%d", static_cast<int>(frame_type), frame_num,
           ack_num, retx);

  // Handle frame based on type
  switch (frame_type) {
    case AshFrameType::DATA: {
      // Check sequence number
      if (frame_num != this->rx_sequence_) {
        ESP_LOGW(TAG, "Out of sequence DATA frame: expected %d, got %d", this->rx_sequence_, frame_num);
        this->send_nak_frame_(this->rx_sequence_);
        return;
      }

      // Check for ACK in DATA frame (piggybacked ACK) BEFORE processing
      // This must happen first because the handler may send new frames
      if (this->tx_buffer_pending_ && ack_num == ((this->tx_pending_frame_num_ + 1) & ASH_MAX_SEQUENCE)) {
        // ackNum means "I expect frame N next" = "I received up to N-1"
        // So if ackNum == pending+1, our pending frame was received
        uint32_t rtt = millis() - this->ack_timer_start_;
        this->update_adaptive_timeout_(rtt);
        this->clear_tx_buffer_();
        ESP_LOGD(TAG, "ACK received (piggybacked in DATA), RTT: %u ms", rtt);
      }

      // Send ACK immediately
      this->send_ack_frame_(frame_num);

      // Increment RX sequence for next frame
      this->increment_rx_sequence_();

      // Extract payload (skip control byte, exclude CRC)
      size_t payload_length = this->rx_buffer_index_ > 3 ? this->rx_buffer_index_ - 3 : 0;
      const uint8_t *payload = this->rx_buffer_.data() + 1;

      // During boot sequence, route to boot handler
      if (this->boot_sequence_active_ && payload_length > 0) {
        this->handle_boot_data_frame_(payload, payload_length);
      } else if (this->api_connection_ != nullptr && payload_length > 0) {
        // Forward EZSP payload to API client
        this->outgoing_proto_msg_.data = payload;
        this->outgoing_proto_msg_.data_len = payload_length;
        this->api_connection_->send_zigbee_proxy_frame(this->outgoing_proto_msg_);
      }
      break;
    }

    case AshFrameType::ACK:
      // Check if this ACKs our pending frame
      // ackNum means "I expect frame N next" = "I received all frames up to N-1"
      // So if ackNum == pending+1, our pending frame was acknowledged
      if (this->tx_buffer_pending_ && ack_num == ((this->tx_pending_frame_num_ + 1) & ASH_MAX_SEQUENCE)) {
        uint32_t rtt = millis() - this->ack_timer_start_;
        this->update_adaptive_timeout_(rtt);
        this->clear_tx_buffer_();
        ESP_LOGD(TAG, "ACK received for frame %d, RTT: %u ms", this->tx_pending_frame_num_, rtt);
      }
      break;

    case AshFrameType::NAK:
      ESP_LOGW(TAG, "NAK received for frame %d, retransmitting", ack_num);
      if (this->tx_buffer_pending_) {
        this->handle_retransmission_();
      }
      break;

    case AshFrameType::RST: {
      ESP_LOGW(TAG, "Received RST frame from NCP, sending RSTACK");
      // Send RSTACK response
      uint8_t rstack_data[] = {0x02, 0x01, 0x00};  // RSTACK with reset code
      this->handle_rstack_frame_(rstack_data, sizeof(rstack_data));
      break;
    }

    case AshFrameType::RSTACK:
      this->handle_rstack_frame_(this->rx_buffer_.data() + 1, this->rx_buffer_index_ - 3);
      break;

    case AshFrameType::ERROR:
      this->handle_error_frame_(this->rx_buffer_.data() + 1, this->rx_buffer_index_ - 3);
      break;
  }
}

bool ZigbeeProxy::parse_byte_(uint8_t byte) {
  // ASH_CAN (0x1A) resets the parser state - discard any partial frame
  static constexpr uint8_t ASH_CAN_BYTE = 0x1A;
  if (byte == ASH_CAN_BYTE) {
    this->rx_buffer_index_ = 0;
    this->escape_next_byte_ = false;
    this->parsing_state_ = ParsingState::WAIT_FLAG_START;
    return false;
  }

  switch (this->parsing_state_) {
    case ParsingState::WAIT_FLAG_START:
      // Handle escape sequences - NCP may send escaped control byte at frame start
      if (byte == ASH_ESCAPE_BYTE) {
        this->escape_next_byte_ = true;
        return false;
      }

      if (this->escape_next_byte_) {
        byte ^= ASH_XOR_BYTE;
        this->escape_next_byte_ = false;
        // After unescaping, check if it's a CAN byte (0x1A)
        if (byte == ASH_CAN_BYTE) {
          this->rx_buffer_index_ = 0;
          return false;
        }
      }

      if (byte == ASH_FLAG_BYTE) {
        // Start of frame with FLAG delimiter
        this->rx_buffer_index_ = 0;
        this->escape_next_byte_ = false;
        this->parsing_state_ = ParsingState::WAIT_CONTROL;
        ESP_LOGV(TAG, "Frame start detected (FLAG)");
      } else if (this->ash_state_ == AshState::CONNECTED) {
        // When connected, NCP often omits leading FLAG on responses
        // Any byte could be a control byte:
        //   - DATA frames: 0x00-0x7F (bit 7 = 0)
        //   - ACK frames:  0x80-0x9F (bits 7-6 = 10, bit 5 = 0)
        //   - NAK frames:  0xA0-0xBF (bits 7-6 = 10, bit 5 = 1)
        //   - RST/RSTACK/ERROR: 0xC0-0xC2 (bits 7-6 = 11)
        // Skip reserved bytes that cannot be valid control bytes
        if (byte != 0x11 && byte != 0x13) {
          this->rx_buffer_index_ = 0;
          this->rx_buffer_[this->rx_buffer_index_++] = byte;
          this->parsing_state_ = ParsingState::WAIT_DATA;
          ESP_LOGV(TAG, "Frame start detected (control byte 0x%02X)", byte);
        }
      } else if ((byte & 0x80) != 0) {
        // Before connected, only accept control/management frames (bit 7 set)
        // This handles RSTACK (0xC1), ACK (0x8X), NAK (0xAX), ERROR (0xC2)
        this->rx_buffer_index_ = 0;
        this->rx_buffer_[this->rx_buffer_index_++] = byte;
        this->parsing_state_ = ParsingState::WAIT_DATA;
        ESP_LOGV(TAG, "Frame start detected (control byte 0x%02X)", byte);
      }
      // Check for bootloader patterns
      this->check_bootloader_mode_(&byte, 1);
      break;

    case ParsingState::WAIT_CONTROL:
      if (byte == ASH_FLAG_BYTE) {
        // Empty frame or repeated FLAG
        ESP_LOGV(TAG, "Empty frame or repeated FLAG, restarting");
        this->rx_buffer_index_ = 0;
        return false;
      }

      if (byte == ASH_ESCAPE_BYTE) {
        this->escape_next_byte_ = true;
        return false;
      }

      if (this->escape_next_byte_) {
        byte ^= ASH_XOR_BYTE;
        this->escape_next_byte_ = false;
      }

      // Store control byte
      this->rx_buffer_[this->rx_buffer_index_++] = byte;
      this->parsing_state_ = ParsingState::WAIT_DATA;
      break;

    case ParsingState::WAIT_DATA:
      if (byte == ASH_FLAG_BYTE) {
        // End of frame - validate and process
        ESP_LOGV(TAG, "Frame complete, %u bytes in buffer", this->rx_buffer_index_);
        if (this->validate_frame_crc_()) {
          this->parse_control_byte_(this->rx_buffer_[0]);
        } else {
          // CRC failed - log frame contents for debugging
          ESP_LOGW(TAG, "CRC failed, frame (%u bytes): %s", this->rx_buffer_index_,
                   format_hex_pretty(this->rx_buffer_.data(), this->rx_buffer_index_).c_str());
          this->send_nak_frame_(this->rx_sequence_);
        }
        this->parsing_state_ = ParsingState::WAIT_FLAG_START;
        return true;
      }

      if (byte == ASH_ESCAPE_BYTE) {
        this->escape_next_byte_ = true;
        return false;
      }

      if (this->escape_next_byte_) {
        byte ^= ASH_XOR_BYTE;
        this->escape_next_byte_ = false;
      }

      // Check buffer overflow
      if (this->rx_buffer_index_ >= MAX_ASH_FRAME_SIZE) {
        ESP_LOGE(TAG, "RX buffer overflow, frame too large");
        this->parsing_state_ = ParsingState::WAIT_FLAG_START;
        return false;
      }

      // Store data byte
      this->rx_buffer_[this->rx_buffer_index_++] = byte;
      break;

    default:
      this->parsing_state_ = ParsingState::WAIT_FLAG_START;
      break;
  }

  return false;
}

size_t ZigbeeProxy::build_frame_(uint8_t *output, const uint8_t *data, size_t length, AshFrameType type,
                                 uint8_t frame_num, uint8_t ack_num, bool retx) {
  size_t pos = 0;

  // Start with FLAG
  output[pos++] = ASH_FLAG_BYTE;

  // Build control byte
  uint8_t control = 0;
  switch (type) {
    case AshFrameType::DATA:
      // DATA frame format: 0ffrPPPP
      // Bit 7 = 0 (DATA indicator), bits 6-4 = frmNum, bit 3 = reTx, bits 2-0 = ackNum
      control = (frame_num << 4) | (retx ? 0x08 : 0x00) | ack_num;
      break;
    case AshFrameType::ACK:
      control = 0x80 | ack_num;
      break;
    case AshFrameType::NAK:
      control = 0xA0 | ack_num;
      break;
    case AshFrameType::RST:
      control = 0xC0;
      break;
    case AshFrameType::RSTACK:
      control = 0xC1;
      break;
    case AshFrameType::ERROR:
      control = 0xC2;
      break;
  }

  // Add control byte with stuffing
  if (control == ASH_FLAG_BYTE || control == ASH_ESCAPE_BYTE || control == 0x11 || control == 0x13 || control == 0x93 ||
      control == 0xA3) {
    output[pos++] = ASH_ESCAPE_BYTE;
    output[pos++] = control ^ ASH_XOR_BYTE;
  } else {
    output[pos++] = control;
  }

  // Prepare CRC calculation buffer (control + data)
  uint8_t crc_buffer[MAX_ASH_FRAME_SIZE];
  crc_buffer[0] = control;
  if (length > 0) {
    memcpy(crc_buffer + 1, data, length);
  }

  // Add data payload with stuffing
  for (size_t i = 0; i < length; i++) {
    uint8_t byte = data[i];
    if (byte == ASH_FLAG_BYTE || byte == ASH_ESCAPE_BYTE || byte == 0x11 || byte == 0x13 || byte == 0x93 ||
        byte == 0xA3) {
      output[pos++] = ASH_ESCAPE_BYTE;
      output[pos++] = byte ^ ASH_XOR_BYTE;
    } else {
      output[pos++] = byte;
    }
  }

  // Calculate CRC
  uint16_t crc = this->calculate_crc_(crc_buffer, 1 + length);

  // Add CRC with stuffing (big-endian)
  uint8_t crc_high = (crc >> 8) & 0xFF;
  uint8_t crc_low = crc & 0xFF;

  if (crc_high == ASH_FLAG_BYTE || crc_high == ASH_ESCAPE_BYTE || crc_high == 0x11 || crc_high == 0x13 ||
      crc_high == 0x93 || crc_high == 0xA3) {
    output[pos++] = ASH_ESCAPE_BYTE;
    output[pos++] = crc_high ^ ASH_XOR_BYTE;
  } else {
    output[pos++] = crc_high;
  }

  if (crc_low == ASH_FLAG_BYTE || crc_low == ASH_ESCAPE_BYTE || crc_low == 0x11 || crc_low == 0x13 || crc_low == 0x93 ||
      crc_low == 0xA3) {
    output[pos++] = ASH_ESCAPE_BYTE;
    output[pos++] = crc_low ^ ASH_XOR_BYTE;
  } else {
    output[pos++] = crc_low;
  }

  // End with FLAG
  output[pos++] = ASH_FLAG_BYTE;

  return pos;
}

}  // namespace zigbee_proxy
}  // namespace esphome

#endif  // USE_ZIGBEE_PROXY
