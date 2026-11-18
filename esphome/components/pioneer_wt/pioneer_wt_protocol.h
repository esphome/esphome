#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace esphome::pioneer_wt {

// Frame layout: A5 01 | flags | type | seq | ack | 00 | len | crc_hi crc_lo | class class | body
static constexpr size_t HEADER_SIZE = 10;
static constexpr size_t MAX_FRAME_SIZE = 256;
static constexpr size_t MAX_BODY_SIZE = 32;
static constexpr size_t TX_QUEUE_SIZE = 4;

static constexpr uint8_t FLAG_NO_ACK = 0x00;
static constexpr uint8_t FLAG_WANT_ACK = 0x01;
static constexpr uint8_t TYPE_DATA = 0x21;
static constexpr uint8_t TYPE_REPLY = 0x23;

static constexpr uint8_t CLASS_COMMAND = 0x0A;
static constexpr uint8_t CLASS_NETWORK = 0x0B;
static constexpr uint8_t CLASS_STATUS = 0x0C;

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

static constexpr uint8_t MODE_AUTO = 0;
static constexpr uint8_t MODE_COOL = 1;
static constexpr uint8_t MODE_DRY = 2;
static constexpr uint8_t MODE_FAN = 3;
static constexpr uint8_t MODE_HEAT = 4;

static constexpr uint8_t SWING_ON = 0x01;
static constexpr uint8_t SWING_OFF = 0x08;

static constexpr uint8_t FAN_SPEED_COUNT = 7;

/// CRC-16/XMODEM over a whole frame, skipping the two checksum bytes at offset 8.
uint16_t frame_crc(const uint8_t *frame, size_t len);

/// Collects bytes from the serial line into complete frames with a valid checksum.
class FrameReader {
 public:
  /// Feeds one byte. Returns the frame length once a complete, valid frame is in frame().
  size_t feed(uint8_t byte);
  const uint8_t *frame() const { return this->buf_.data(); }

 protected:
  std::array<uint8_t, MAX_FRAME_SIZE> buf_{};
  size_t pos_{0};
};

/// Walks the attribute list in a status body. Stops at the first ID whose size is unknown.
class AttributeReader {
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
  uint16_t id_{0};
  uint32_t value_{0};
  bool unknown_{false};
};

/// Builds a frame into out. payload is everything after the checksum. Returns the frame length, or 0 if it won't fit.
size_t build_frame(uint8_t *out, size_t out_size, uint8_t flags, uint8_t type, uint8_t seq, uint8_t ack,
                   const uint8_t *payload, size_t len);
/// Appends one attribute (2-byte ID, then a big-endian value of size bytes) at pos. Returns the new end.
size_t put_attr(uint8_t *buf, size_t pos, uint16_t id, uint32_t value, uint8_t size);

}  // namespace esphome::pioneer_wt
