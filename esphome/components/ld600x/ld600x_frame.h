#pragma once

#include <cstddef>
#include <cstdint>

#include "esphome/core/helpers.h"

namespace esphome::ld600x {

static constexpr uint8_t TF_SOF = 0x01;
// ID, payload length and type, excluding SOF and the header checksum.
static constexpr uint8_t TF_HEADER_LEN = 6;
// SOF, header, header checksum and data checksum around a payload.
static constexpr uint8_t TF_FRAME_OVERHEAD = TF_HEADER_LEN + 3;

inline float read_f32_le(const uint8_t *data) { return bit_cast<float>(encode_value_le<uint32_t>(data)); }

inline void write_u32_le(uint8_t *data, uint32_t value) {
  data[0] = value & 0xFF;
  data[1] = (value >> 8) & 0xFF;
  data[2] = (value >> 16) & 0xFF;
  data[3] = (value >> 24) & 0xFF;
}

inline void write_f32_le(uint8_t *data, float value) { write_u32_le(data, bit_cast<uint32_t>(value)); }

class FrameParser final {
 public:
  // The caller owns the buffer; payloads longer than capacity are skipped.
  void init(uint8_t *buffer, uint16_t capacity);
  bool feed(uint8_t byte);
  uint16_t type() const { return this->frame_type_; }
  uint16_t id() const { return this->frame_id_; }
  uint16_t length() const { return this->data_len_; }
  const uint8_t *data() const { return this->data_len_ == 0 ? nullptr : this->data_buf_; }
  uint16_t capacity() const { return this->max_data_len_; }
  void reset() { this->parse_state_ = ParseState::PARSE_STATE_SOF; }

 protected:
  enum class ParseState : uint8_t {
    PARSE_STATE_SOF,
    PARSE_STATE_HEADER,
    PARSE_STATE_HCK,
    PARSE_STATE_DATA,
    PARSE_STATE_DCK,
    PARSE_STATE_DISCARD,
  };

  void start_header_();

  uint8_t *data_buf_{nullptr};
  uint16_t data_len_{0};
  uint16_t frame_type_{0};
  uint16_t frame_id_{0};
  // Header or payload write position, or the payload bytes left to discard.
  uint16_t pos_{0};
  uint16_t max_data_len_{0};
  ParseState parse_state_{ParseState::PARSE_STATE_SOF};
  uint8_t xor_{0};
};

// Frames this side sends have the top id bit set; the low 15 bits count up.
inline uint16_t next_frame_id(uint16_t &counter) { return (counter++ & 0x7FFF) | 0x8000; }

// Out must hold TF_FRAME_OVERHEAD + len bytes. Empty payloads have no data checksum.
size_t encode_frame(uint16_t frame_id, uint16_t type, const uint8_t *data, uint16_t len, uint8_t *out);

}  // namespace esphome::ld600x
