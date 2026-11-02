#include "ld600x_frame.h"

#include "esphome/core/log.h"

#include <cstring>

namespace esphome::ld600x {

ESPHOME_LOG_TAG(TAG, "ld600x");

void FrameParser::init(uint8_t *buffer, uint16_t capacity) {
  this->data_buf_ = buffer;
  this->max_data_len_ = capacity;
  this->reset();
}

void FrameParser::start_header_() {
  this->pos_ = 0;
  this->xor_ = TF_SOF;
  this->parse_state_ = ParseState::PARSE_STATE_HEADER;
}

bool FrameParser::feed(uint8_t byte) {
  switch (this->parse_state_) {
    case ParseState::PARSE_STATE_DISCARD:
      // pos_ counts the payload down; the byte after it is the data checksum.
      if (this->pos_ == 0) {
        this->reset();
      } else {
        this->pos_--;
      }
      return false;
    case ParseState::PARSE_STATE_SOF:
      if (byte == TF_SOF)
        this->start_header_();
      return false;
    case ParseState::PARSE_STATE_HEADER: {
      // Big-endian id, length and type, shifted in as they arrive so the buffer only ever holds a payload
      uint16_t &field = this->pos_ < 2 ? this->frame_id_ : this->pos_ < 4 ? this->data_len_ : this->frame_type_;
      field = static_cast<uint16_t>((field << 8) | byte);
      this->xor_ ^= byte;
      if (++this->pos_ == TF_HEADER_LEN)
        this->parse_state_ = ParseState::PARSE_STATE_HCK;
      return false;
    }
    case ParseState::PARSE_STATE_DATA:
      this->data_buf_[this->pos_++] = byte;
      this->xor_ ^= byte;
      if (this->pos_ >= this->data_len_) {
        this->parse_state_ = ParseState::PARSE_STATE_DCK;
      }
      return false;
    case ParseState::PARSE_STATE_HCK:
    case ParseState::PARSE_STATE_DCK: {
      const bool header = this->parse_state_ == ParseState::PARSE_STATE_HCK;
      if (byte != static_cast<uint8_t>(~this->xor_)) {
        ESP_LOGV(TAG, "%s checksum mismatch", header ? LOG_STR_LITERAL("Header") : LOG_STR_LITERAL("Data"));
        // The byte may be the SOF of the next frame when the real checksum was lost on the wire.
        if (byte == TF_SOF) {
          this->start_header_();
        } else {
          this->reset();
        }
        return false;
      }
      if (!header || this->data_len_ == 0) {
        this->reset();
        return true;
      }
      if (this->data_len_ > this->max_data_len_) {
        ESP_LOGW(TAG, "Frame too large: %u", static_cast<unsigned>(this->data_len_));
        this->pos_ = this->data_len_;
        this->parse_state_ = ParseState::PARSE_STATE_DISCARD;
        return false;
      }
      this->pos_ = 0;
      this->xor_ = 0;
      this->parse_state_ = ParseState::PARSE_STATE_DATA;
      return false;
    }
  }
  return false;
}

size_t encode_frame(uint16_t frame_id, uint16_t type, const uint8_t *data, uint16_t len, uint8_t *out) {
  out[0] = TF_SOF;
  out[1] = frame_id >> 8;
  out[2] = frame_id & 0xFF;
  out[3] = len >> 8;
  out[4] = len & 0xFF;
  out[5] = type >> 8;
  out[6] = type & 0xFF;
  // TinyFrame checksums are the inverted XOR of the bytes they cover, SOF included for the header.
  out[TF_HEADER_LEN + 1] = static_cast<uint8_t>(~xor_bytes(out, TF_HEADER_LEN + 1));
  size_t pos = TF_PAYLOAD_OFFSET;
  if (len > 0 && data != nullptr) {
    std::memcpy(out + pos, data, len);
    pos += len;
    out[pos++] = static_cast<uint8_t>(~xor_bytes(data, len));
  }
  return pos;
}

}  // namespace esphome::ld600x
