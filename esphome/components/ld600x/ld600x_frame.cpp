#include "ld600x_frame.h"

#include <cstring>

namespace esphome::ld600x {

uint16_t read_u16_be(const uint8_t *data) { return (static_cast<uint16_t>(data[0]) << 8) | data[1]; }

uint32_t read_u32_le(const uint8_t *data) {
  return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

int32_t read_int32_le(const uint8_t *data) {
  uint32_t raw = read_u32_le(data);
  int32_t value;
  std::memcpy(&value, &raw, sizeof(value));
  return value;
}

float read_f32_le(const uint8_t *data) {
  uint32_t raw = read_u32_le(data);
  float value;
  std::memcpy(&value, &raw, sizeof(value));
  return value;
}

void write_u32_le(uint8_t *data, uint32_t value) {
  data[0] = value & 0xFF;
  data[1] = (value >> 8) & 0xFF;
  data[2] = (value >> 16) & 0xFF;
  data[3] = (value >> 24) & 0xFF;
}

void write_int32_le(uint8_t *data, int32_t value) { write_u32_le(data, static_cast<uint32_t>(value)); }

void write_f32_le(uint8_t *data, float value) {
  uint32_t raw;
  std::memcpy(&raw, &value, sizeof(raw));
  write_u32_le(data, raw);
}

void FrameParser::init(uint8_t *buffer, size_t capacity) {
  this->data_buf_ = buffer;
  this->max_data_len_ = capacity;
  this->reset();
}

void FrameParser::reset() {
  this->parse_state_ = ParseState::PARSE_STATE_SOF;
  this->header_pos_ = 0;
  this->header_xor_ = 0;
  this->data_len_ = 0;
  this->data_pos_ = 0;
  this->data_xor_ = 0;
  this->discard_remaining_ = 0;
  this->frame_oversize_ = false;
}

bool FrameParser::feed(uint8_t byte) {
  this->event_ = FrameEvent::FRAME_EVENT_NONE;
  switch (this->parse_state_) {
    case ParseState::PARSE_STATE_DISCARD:
      // discard_remaining_ is unsigned: an unguarded decrement at zero would swallow 4 GB of stream.
      if (this->discard_remaining_ > 0) {
        this->discard_remaining_--;
      }
      if (this->discard_remaining_ == 0) {
        this->reset();
      }
      return false;
    case ParseState::PARSE_STATE_SOF:
      if (byte != TF_SOF)
        return false;
      this->header_pos_ = 0;
      this->header_xor_ = 0;
      this->header_xor_ ^= byte;
      this->parse_state_ = ParseState::PARSE_STATE_HEADER;
      return false;
    case ParseState::PARSE_STATE_HEADER:
      if (this->header_pos_ < TF_HEADER_LEN) {
        this->data_buf_[this->header_pos_] = byte;
        this->header_xor_ ^= byte;
        this->header_pos_++;
        if (this->header_pos_ == TF_HEADER_LEN) {
          this->frame_id_ = read_u16_be(this->data_buf_);
          this->data_len_ = read_u16_be(this->data_buf_ + 2);
          this->frame_length_ = this->data_len_;
          this->frame_type_ = read_u16_be(this->data_buf_ + 4);
          // The length is only trustworthy once the header checksum has been verified, so just
          // remember that the frame is oversized and let the HCK state act on it.
          this->frame_oversize_ = this->data_len_ > this->max_data_len_;
          this->parse_state_ = ParseState::PARSE_STATE_HCK;
        }
      }
      return false;
    case ParseState::PARSE_STATE_HCK: {
      uint8_t expected = static_cast<uint8_t>(~this->header_xor_);
      if (byte != expected) {
        this->event_ = FrameEvent::FRAME_EVENT_HEADER_CHECKSUM_MISMATCH;
        this->reset();
        return false;
      }
      if (this->frame_oversize_) {
        this->event_ = FrameEvent::FRAME_EVENT_OVERSIZED;
        // The header is verified, so the length can be trusted: skip the payload and its checksum.
        this->discard_remaining_ = static_cast<uint32_t>(this->data_len_) + 1;
        this->parse_state_ = ParseState::PARSE_STATE_DISCARD;
        return false;
      }
      if (this->data_len_ == 0) {
        this->reset();
        return true;
      } else {
        this->data_pos_ = 0;
        this->data_xor_ = 0;
        this->parse_state_ = ParseState::PARSE_STATE_DATA;
      }
      return false;
    }
    case ParseState::PARSE_STATE_DATA:
      this->data_buf_[this->data_pos_++] = byte;
      this->data_xor_ ^= byte;
      if (this->data_pos_ >= this->data_len_) {
        this->parse_state_ = ParseState::PARSE_STATE_DCK;
      }
      return false;
    case ParseState::PARSE_STATE_DCK: {
      uint8_t expected = static_cast<uint8_t>(~this->data_xor_);
      bool valid = byte == expected;
      if (!valid) {
        this->event_ = FrameEvent::FRAME_EVENT_DATA_CHECKSUM_MISMATCH;
      }
      this->reset();
      return valid;
    }
  }
  return false;
}

size_t encode_frame(uint16_t frame_id, uint16_t type, const uint8_t *data, uint8_t len, uint8_t *out) {
  size_t pos = 0;
  uint8_t header_xor = 0;
  auto write_header = [&](uint8_t b) {
    out[pos++] = b;
    header_xor ^= b;
  };

  write_header(TF_SOF);
  write_header((frame_id >> 8) & 0xFF);
  write_header(frame_id & 0xFF);
  write_header((len >> 8) & 0xFF);
  write_header(len & 0xFF);
  write_header((type >> 8) & 0xFF);
  write_header(type & 0xFF);

  out[pos++] = static_cast<uint8_t>(~header_xor);

  if (len > 0 && data != nullptr) {
    uint8_t data_xor = 0;
    for (uint8_t i = 0; i < len; i++) {
      out[pos++] = data[i];
      data_xor ^= data[i];
    }
    out[pos++] = static_cast<uint8_t>(~data_xor);
  }
  return pos;
}

}  // namespace esphome::ld600x
