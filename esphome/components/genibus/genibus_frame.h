#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace esphome::genibus {

// Start delimiters (SD)
static constexpr uint8_t START_REQUEST = 0x27;
static constexpr uint8_t START_REPLY = 0x24;
static constexpr uint8_t START_MESSAGE = 0x26;

// SD, LE, DA, SA, ..., CRC high, CRC low; LE counts DA up to the last data byte
static constexpr size_t FRAME_OVERHEAD = 4;
static constexpr size_t MIN_FRAME_SIZE = FRAME_OVERHEAD + 2;
static constexpr size_t MAX_FRAME_SIZE = FRAME_OVERHEAD + 255;

static constexpr uint8_t CLASS_DATA_OBJECT = 10;

// Operation in bits 7-6 of the second APDU byte of a request; bits 5-0 hold the data length
static constexpr uint8_t OPERATION_GET = 0x00;
static constexpr uint8_t APDU_LENGTH_MASK = 0x3F;

// status, type (2), version, size (3)
static constexpr size_t DATA_OBJECT_HEADER_SIZE = 7;

// ACK code of a reply APDU: the data item ID is unknown
static constexpr uint8_t ACK_UNKNOWN_ID = 2;
// The only data byte of a class 10 reply to a GET of an object the device does not have
static constexpr uint8_t DATA_OBJECT_STATUS_MISSING = 4;

/// CRC-16/GENIBUS of LE up to the last data byte.
uint16_t frame_crc(const uint8_t *data, size_t len);

/// Build a request frame around apdus into out; returns its size, or 0 when it does not fit.
size_t build_request(std::span<uint8_t> out, uint8_t destination, uint8_t source, std::span<const uint8_t> apdus);

/// Write a class 10 GET APDU of object/sub_id (5 bytes: class, length, object, sub ID) and return the pointer past
/// it. The 2005 specification does not describe class 10; this is the request the Grundfos GO app and the alpha3
/// component send, and a real ALPHA3 answers it.
uint8_t *put_get_object(uint8_t *dst, uint8_t object, uint16_t sub_id);

/// One APDU; data points into the frame.
struct Apdu {
  uint8_t data_class;
  uint8_t ack;  // bits 7-6 of the second byte: ACK code of a reply (0 ok), operation of a request
  std::span<const uint8_t> data;
};

/// Walks the APDUs of a frame's data.
class ApduReader {
 public:
  explicit ApduReader(std::span<const uint8_t> apdus) : rest_(apdus) {}
  /// Read the next APDU; false at the end or when its length does not fit.
  bool next(Apdu &apdu);
  bool at_end() const { return this->rest_.empty(); }

 protected:
  std::span<const uint8_t> rest_;
};

/// A class 10 data object of a reply; body points into the frame.
struct DataObject {
  uint8_t status;
  uint16_t type;
  uint8_t version;
  std::span<const uint8_t> body;
};

/// Parse a class 10 reply APDU with ACK 0 whose size field matches its body.
bool parse_data_object(const Apdu &apdu, DataObject &object);

/// Whether a class 10 reply to a GET says the device does not have the object. An ALPHA3 answers with ACK 0 and the
/// single data byte 4 (24 05 F8 E7 0A 01 04 EE 26); the 2005 specification has ACK 2 for an unknown ID. Use it only on
/// the reply to a GET: to a SET, a single data byte 4 means the write failed.
bool is_object_missing(const Apdu &apdu);

/// Collects bytes that may arrive in any chunks into one frame.
class FrameAssembler {
 public:
  /// Add a byte; true when it completes a frame. Bytes before a start delimiter are dropped, and so is a complete
  /// frame when the next byte comes before reset().
  bool feed(uint8_t byte);
  void reset() { this->size_ = 0; }
  std::span<const uint8_t> frame() const { return {this->buffer_.data(), this->size_}; }
  bool crc_ok() const;
  /// The APDUs of a complete frame; empty while fewer than MIN_FRAME_SIZE bytes are in.
  std::span<const uint8_t> apdus() const {
    if (this->size_ < MIN_FRAME_SIZE)
      return {};
    return this->frame().subspan(4, this->size_ - MIN_FRAME_SIZE);
  }

 protected:
  std::array<uint8_t, MAX_FRAME_SIZE> buffer_{};
  uint16_t size_{0};
};

}  // namespace esphome::genibus
