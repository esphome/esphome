#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::ld600x {

static constexpr uint8_t TF_SOF = 0x01;
// ID, payload length and type, excluding SOF and the header checksum.
static constexpr uint8_t TF_HEADER_LEN = 6;

uint16_t read_u16_be(const uint8_t *data);
uint32_t read_u32_le(const uint8_t *data);
int32_t read_int32_le(const uint8_t *data);
float read_f32_le(const uint8_t *data);
void write_u32_le(uint8_t *data, uint32_t value);
void write_int32_le(uint8_t *data, int32_t value);
void write_f32_le(uint8_t *data, float value);

enum class FrameEvent : uint8_t {
  FRAME_EVENT_NONE,
  FRAME_EVENT_HEADER_CHECKSUM_MISMATCH,
  FRAME_EVENT_DATA_CHECKSUM_MISMATCH,
  FRAME_EVENT_OVERSIZED,
};

class FrameParser final {
 public:
  // The caller owns the buffer. Capacity must be at least TF_HEADER_LEN.
  void init(uint8_t *buffer, size_t capacity);
  bool feed(uint8_t byte);
  uint16_t type() const { return this->frame_type_; }
  uint16_t id() const { return this->frame_id_; }
  uint16_t length() const { return this->frame_length_; }
  const uint8_t *data() const { return this->frame_length_ == 0 ? nullptr : this->data_buf_; }
  // Reports the most recent feed call, including errors that reset the parser.
  FrameEvent event() const { return this->event_; }
  void reset();

 protected:
  enum class ParseState : uint8_t {
    PARSE_STATE_SOF,
    PARSE_STATE_HEADER,
    PARSE_STATE_HCK,
    PARSE_STATE_DATA,
    PARSE_STATE_DCK,
    PARSE_STATE_DISCARD,
  };

  ParseState parse_state_{ParseState::PARSE_STATE_SOF};
  uint8_t header_pos_{0};
  uint8_t header_xor_{0};
  uint16_t data_len_{0};
  uint16_t frame_type_{0};
  uint16_t frame_id_{0};
  uint16_t data_pos_{0};
  uint8_t data_xor_{0};
  uint32_t discard_remaining_{0};
  bool frame_oversize_{false};
  size_t max_data_len_{0};
  uint8_t *data_buf_{nullptr};
  // Retained across reset so the completed frame remains readable until the next feed.
  uint16_t frame_length_{0};
  FrameEvent event_{FrameEvent::FRAME_EVENT_NONE};
};

// Out must hold 8 + len + 1 bytes. Empty payloads have no data checksum.
size_t encode_frame(uint16_t frame_id, uint16_t type, const uint8_t *data, uint8_t len, uint8_t *out);

}  // namespace esphome::ld600x
