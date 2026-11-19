#pragma once

#include <cstddef>
#include <cstdint>

#include "esphome/core/frame_reader.h"
#include "esphome/core/helpers.h"

namespace esphome::pioneer_wt {

// Frame layout: A5 01 | flags | type | seq | ack | 00 | len | crc_hi crc_lo | class class | body
static constexpr size_t HEADER_SIZE = 10;
static constexpr uint8_t FRAME_START_1 = 0xA5;
static constexpr uint8_t FRAME_START_2 = 0x01;
// The length byte counts the whole frame.
static constexpr size_t LEN_POS = 7;
static constexpr size_t CRC_POS = 8;
static constexpr size_t MAX_FRAME_SIZE = 255;

static constexpr uint8_t FLAG_NO_ACK = 0x00;
static constexpr uint8_t FLAG_WANT_ACK = 0x01;
static constexpr uint8_t TYPE_DATA = 0x21;
static constexpr uint8_t TYPE_REPLY = 0x23;

static constexpr uint8_t CLASS_COMMAND = 0x0A;
static constexpr uint8_t CLASS_NETWORK = 0x0B;
static constexpr uint8_t CLASS_STATUS = 0x0C;
// First class byte of a reply
static constexpr uint8_t CLASS_REPLY = 0x80;

// Attribute IDs
static constexpr uint16_t ATTR_POWER = 0x0001;
static constexpr uint16_t ATTR_SETPOINT_C100 = 0x0002;
static constexpr uint16_t ATTR_INDOOR_TEMP_C100 = 0x0003;
static constexpr uint16_t ATTR_FAN_SPEED = 0x0005;
static constexpr uint16_t ATTR_SWING_VERTICAL = 0x0011;
static constexpr uint16_t ATTR_MODE = 0x0012;
static constexpr uint16_t ATTR_ECO = 0x0013;
static constexpr uint16_t ATTR_DISPLAY = 0x001E;
static constexpr uint16_t ATTR_BEEPER = 0x0025;
static constexpr uint16_t ATTR_FAN_AUTO = 0x0073;
static constexpr uint16_t ATTR_FAN_EXTRA_1 = 0x0224;
static constexpr uint16_t ATTR_FAN_EXTRA_2 = 0x0225;
static constexpr uint16_t ATTR_SETPOINT_F = 0x0227;

static constexpr uint8_t SWING_ON = 0x01;
static constexpr uint8_t SWING_OFF = 0x08;

/// CRC-16/XMODEM over a whole frame, skipping the two checksum bytes at offset 8.
uint16_t frame_crc(const uint8_t *frame, size_t len);

// A5 01, ..., whole-frame length at 7, CRC (2) at 8, body
using PacketReader =
    LengthFrameReader<MAX_FRAME_SIZE,
                      FrameFormat<FrameHeader<FRAME_START_1, FRAME_START_2>,
                                  FrameLengthU8<LengthField{.offset = LEN_POS}>, FrameMinLength<HEADER_SIZE>>>;

inline bool checksum_valid(const uint8_t *frame, size_t len) {
  return frame_crc(frame, len) == encode_uint16(frame[CRC_POS], frame[CRC_POS + 1]);
}

/// Walks the attribute list in a status body. Stops at the first ID whose size is unknown.
class AttributeReader final {
 public:
  AttributeReader(const uint8_t *data, size_t len) : data_(data), len_(len) {}
  /// Moves to the next attribute. Returns false at the end of the body or at an unknown ID.
  bool next();
  uint16_t id() const { return this->id_; }
  uint32_t value() const { return this->value_; }
  /// True when the walk stopped early on an ID of unknown size.
  bool stopped_on_unknown() const { return this->unknown_; }

 protected:
  const uint8_t *data_;
  size_t len_;
  size_t pos_{0};
  uint32_t value_{0};
  uint16_t id_{0};
  bool unknown_{false};
};

/// Builds a frame into out, which must hold HEADER_SIZE + 2 + len bytes. Returns the frame length.
size_t build_frame(uint8_t *out, uint8_t flags, uint8_t type, uint8_t seq, uint8_t ack, uint8_t class_1,
                   uint8_t class_2, const uint8_t *body, size_t len);

/// Writes a one-byte attribute (2-byte ID, then the value) at p. Returns the new end.
inline uint8_t *put_attr(uint8_t *p, uint16_t id, uint8_t value) {
  p = put_be16(p, id);
  *p++ = value;
  return p;
}

}  // namespace esphome::pioneer_wt
