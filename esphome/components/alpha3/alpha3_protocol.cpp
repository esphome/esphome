#include "alpha3_protocol.h"

#include <cstring>

namespace esphome::alpha3 {
namespace {

constexpr size_t FRAME_OVERHEAD = 8;
constexpr size_t FRAME_LENGTH_OFFSET = 1;
constexpr size_t FRAME_DESTINATION_OFFSET = 2;
constexpr size_t FRAME_SOURCE_OFFSET = 3;
constexpr size_t FRAME_CLASS_OFFSET = 4;
constexpr size_t FRAME_OPERATION_OFFSET = 5;
constexpr size_t FRAME_APDU_OFFSET = 6;
constexpr size_t FRAME_CRC_SIZE = 2;
constexpr size_t OBJECT_RESPONSE_HEADER_SIZE = 7;

bool build_frame(uint8_t destination, uint8_t data_class, GeniOperation operation, const uint8_t *apdu,
                 size_t apdu_size, EncodedFrame &output) {
  output = {};
  if (apdu_size > 0x3FU || FRAME_OVERHEAD + apdu_size > MAX_FRAME_SIZE || (apdu_size > 0 && apdu == nullptr))
    return false;

  const size_t frame_size = FRAME_OVERHEAD + apdu_size;
  output.data[0] = GENI_REQUEST_START;
  output.data[FRAME_LENGTH_OFFSET] = static_cast<uint8_t>(frame_size - 4);
  output.data[FRAME_DESTINATION_OFFSET] = destination;
  output.data[FRAME_SOURCE_OFFSET] = GENI_REQUEST_SOURCE;
  output.data[FRAME_CLASS_OFFSET] = data_class;
  output.data[FRAME_OPERATION_OFFSET] = static_cast<uint8_t>((static_cast<uint8_t>(operation) << 6) | apdu_size);
  if (apdu_size > 0) {
    std::memcpy(output.data.data() + FRAME_APDU_OFFSET, apdu, apdu_size);
  }

  const uint16_t crc = geni_crc16(output.data.data() + FRAME_LENGTH_OFFSET, frame_size - 3);
  output.data[frame_size - FRAME_CRC_SIZE] = static_cast<uint8_t>(crc >> 8);
  output.data[frame_size - 1] = static_cast<uint8_t>(crc);
  output.size = static_cast<uint8_t>(frame_size);
  return true;
}

}  // namespace

uint16_t geni_crc16(const uint8_t *data, size_t size) {
  if (data == nullptr)
    return 0;
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < size; i++) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      if ((crc & 0x8000U) != 0) {
        crc = static_cast<uint16_t>((crc << 1) ^ 0x1021U);
      } else {
        crc = static_cast<uint16_t>(crc << 1);
      }
    }
  }
  return static_cast<uint16_t>(~crc);
}

bool build_discovery_get(EncodedFrame &output) {
  // Class 4 carries the verified compound discovery request, not a single APDU.
  output = {{{0x27, 0x0F, GENI_BROADCAST_ADDRESS, GENI_REQUEST_SOURCE, GENI_DISCOVERY_CLASS, 0x02, 0x2E, 0x2F, 0x02,
              0x03, 0x94, 0x95, 0x96, 0x00, 0x02, 0x02, 0x03, 0x4A, 0x4C}},
            19};
  return true;
}

bool build_parameter_get(uint8_t destination, uint8_t parameter_id, EncodedFrame &output) {
  return build_frame(destination, GENI_PARAMETER_CLASS, GeniOperation::GENI_OPERATION_GET, &parameter_id, 1, output);
}

bool build_object_get(uint8_t destination, ObjectAddress address, EncodedFrame &output) {
  const std::array<uint8_t, 3> apdu{
      {address.data_id, static_cast<uint8_t>(address.sub_id >> 8), static_cast<uint8_t>(address.sub_id)}};
  return build_frame(destination, GENI_OBJECT_CLASS, GeniOperation::GENI_OPERATION_GET, apdu.data(), apdu.size(),
                     output);
}

bool build_object_set(uint8_t destination, ObjectAddress address, uint16_t type, uint8_t version,
                      const uint8_t *payload, size_t payload_size, EncodedFrame &output) {
  constexpr size_t object_set_header_size = 9;
  std::array<uint8_t, 0x3F> apdu{};
  if (payload_size > apdu.size() - object_set_header_size || (payload_size > 0 && payload == nullptr)) {
    output = {};
    return false;
  }

  apdu[0] = address.data_id;
  apdu[1] = static_cast<uint8_t>(address.sub_id >> 8);
  apdu[2] = static_cast<uint8_t>(address.sub_id);
  apdu[3] = static_cast<uint8_t>(type >> 8);
  apdu[4] = static_cast<uint8_t>(type);
  apdu[5] = version;
  apdu[6] = static_cast<uint8_t>(payload_size >> 16);
  apdu[7] = static_cast<uint8_t>(payload_size >> 8);
  apdu[8] = static_cast<uint8_t>(payload_size);
  if (payload_size > 0) {
    std::memcpy(apdu.data() + object_set_header_size, payload, payload_size);
  }
  return build_frame(destination, GENI_OBJECT_CLASS, GeniOperation::GENI_OPERATION_SET, apdu.data(),
                     object_set_header_size + payload_size, output);
}

namespace {

ParseResult validate_response_envelope(const uint8_t *data, size_t size) {
  if (data == nullptr || size == 0)
    return ParseResult::PARSE_RESULT_INCOMPLETE;
  if (data[0] != GENI_RESPONSE_START)
    return ParseResult::PARSE_RESULT_INVALID_START;
  if (size < 2)
    return ParseResult::PARSE_RESULT_INCOMPLETE;

  const size_t expected_size = static_cast<size_t>(data[FRAME_LENGTH_OFFSET]) + 4;
  if (expected_size < FRAME_OVERHEAD || expected_size > MAX_FRAME_SIZE)
    return ParseResult::PARSE_RESULT_INVALID_LENGTH;
  if (size < expected_size)
    return ParseResult::PARSE_RESULT_INCOMPLETE;
  if (size > expected_size)
    return ParseResult::PARSE_RESULT_INVALID_LENGTH;
  if (data[FRAME_DESTINATION_OFFSET] != GENI_RESPONSE_DESTINATION ||
      data[FRAME_SOURCE_OFFSET] == GENI_BROADCAST_ADDRESS)
    return ParseResult::PARSE_RESULT_INVALID_ADDRESS;

  const uint16_t expected_crc = geni_crc16(data + FRAME_LENGTH_OFFSET, expected_size - 3);
  const uint16_t received_crc = static_cast<uint16_t>(data[expected_size - 2] << 8) | data[expected_size - 1];
  if (received_crc != expected_crc)
    return ParseResult::PARSE_RESULT_INVALID_CRC;
  return ParseResult::PARSE_RESULT_OK;
}

}  // namespace

ParseResult parse_discovery_response(const uint8_t *data, size_t size, uint8_t &unit_address) {
  unit_address = GENI_BROADCAST_ADDRESS;
  const ParseResult result = validate_response_envelope(data, size);
  if (result != ParseResult::PARSE_RESULT_OK)
    return result;
  if (data[FRAME_CLASS_OFFSET] != GENI_DISCOVERY_CLASS)
    return ParseResult::PARSE_RESULT_UNEXPECTED_CLASS;
  if ((data[FRAME_OPERATION_OFFSET] >> 6) != static_cast<uint8_t>(GeniOperation::GENI_OPERATION_GET))
    return ParseResult::PARSE_RESULT_INVALID_OPERATION;

  // The compound reply's family/type/version fields vary by unit. Independent
  // class-2 reads remain the authority for profile and write selection.
  if (size != FRAME_OPERATION_OFFSET + 12 + FRAME_CRC_SIZE || data[FRAME_OPERATION_OFFSET] != 0x02 ||
      data[FRAME_OPERATION_OFFSET + 1] != data[FRAME_SOURCE_OFFSET] || data[FRAME_OPERATION_OFFSET + 2] != 0xF7 ||
      data[FRAME_OPERATION_OFFSET + 3] != 0x02 || data[FRAME_OPERATION_OFFSET + 4] != 0x03 ||
      data[FRAME_OPERATION_OFFSET + 8] != 0x00 || data[FRAME_OPERATION_OFFSET + 9] != 0x02 ||
      data[FRAME_OPERATION_OFFSET + 10] != 0xFF || data[FRAME_OPERATION_OFFSET + 11] != 0x58)
    return ParseResult::PARSE_RESULT_INVALID_APDU_LENGTH;
  unit_address = data[FRAME_SOURCE_OFFSET];
  return ParseResult::PARSE_RESULT_OK;
}

ParseResult parse_response_frame(const uint8_t *data, size_t size, uint8_t expected_source, ParsedFrame &output) {
  output = {};
  const ParseResult result = validate_response_envelope(data, size);
  if (result != ParseResult::PARSE_RESULT_OK)
    return result;
  if (data[FRAME_SOURCE_OFFSET] != expected_source)
    return ParseResult::PARSE_RESULT_INVALID_ADDRESS;

  const uint8_t operation = data[FRAME_OPERATION_OFFSET] >> 6;
  if (operation != static_cast<uint8_t>(GeniOperation::GENI_OPERATION_GET) &&
      operation != static_cast<uint8_t>(GeniOperation::GENI_OPERATION_SET))
    return ParseResult::PARSE_RESULT_INVALID_OPERATION;
  const uint8_t apdu_size = data[FRAME_OPERATION_OFFSET] & 0x3FU;
  if (apdu_size != data[FRAME_LENGTH_OFFSET] - 4)
    return ParseResult::PARSE_RESULT_INVALID_APDU_LENGTH;

  output.data_class = data[FRAME_CLASS_OFFSET];
  output.operation = static_cast<GeniOperation>(operation);
  output.data = data + FRAME_APDU_OFFSET;
  output.data_size = apdu_size;
  return ParseResult::PARSE_RESULT_OK;
}

ParseResult parse_object_response(const ParsedFrame &frame, uint16_t expected_type, const ObjectVersionSpec *versions,
                                  size_t version_count, ParsedObject &output) {
  output = {};
  if (frame.data_class != GENI_OBJECT_CLASS)
    return ParseResult::PARSE_RESULT_UNEXPECTED_CLASS;
  if (frame.data == nullptr || frame.data_size < OBJECT_RESPONSE_HEADER_SIZE)
    return ParseResult::PARSE_RESULT_INVALID_PAYLOAD_LENGTH;
  if (frame.data[0] != 0)
    return ParseResult::PARSE_RESULT_INVALID_ACK;

  const uint16_t type = static_cast<uint16_t>(frame.data[1] << 8) | frame.data[2];
  if (type != expected_type)
    return ParseResult::PARSE_RESULT_UNEXPECTED_TYPE;
  const uint8_t version = frame.data[3];
  const ObjectVersionSpec *version_spec = nullptr;
  for (size_t i = 0; i < version_count; i++) {
    if (versions != nullptr && versions[i].version == version) {
      version_spec = &versions[i];
      break;
    }
  }
  if (version_spec == nullptr)
    return ParseResult::PARSE_RESULT_UNEXPECTED_VERSION;

  const uint32_t payload_size =
      (static_cast<uint32_t>(frame.data[4]) << 16) | (static_cast<uint32_t>(frame.data[5]) << 8) | frame.data[6];
  if (payload_size != static_cast<uint32_t>(frame.data_size - OBJECT_RESPONSE_HEADER_SIZE))
    return ParseResult::PARSE_RESULT_INVALID_PAYLOAD_LENGTH;
  if (payload_size < version_spec->payload_size ||
      (!version_spec->allow_longer && payload_size != version_spec->payload_size))
    return ParseResult::PARSE_RESULT_INVALID_PAYLOAD_LENGTH;

  output.type = type;
  output.version = version;
  output.payload = frame.data + OBJECT_RESPONSE_HEADER_SIZE;
  output.payload_size = payload_size;
  return ParseResult::PARSE_RESULT_OK;
}

ParseResult parse_write_ack(const ParsedFrame &frame) {
  if (frame.data_class != GENI_OBJECT_CLASS)
    return ParseResult::PARSE_RESULT_UNEXPECTED_CLASS;
  if (frame.data == nullptr || frame.data_size != 1)
    return ParseResult::PARSE_RESULT_INVALID_PAYLOAD_LENGTH;
  if (frame.data[0] != 0)
    return ParseResult::PARSE_RESULT_INVALID_ACK;
  return ParseResult::PARSE_RESULT_OK;
}

void FrameAssembler::reset() {
  this->data_ = {};
  this->size_ = 0;
  this->expected_size_ = 0;
}

ParseResult FrameAssembler::append(const uint8_t *fragment, size_t fragment_size) {
  if (fragment_size == 0)
    return this->complete() ? ParseResult::PARSE_RESULT_OK : ParseResult::PARSE_RESULT_INCOMPLETE;
  if (fragment == nullptr || this->complete())
    return ParseResult::PARSE_RESULT_OVERFLOW;

  for (size_t i = 0; i < fragment_size; i++) {
    if (this->size_ >= MAX_FRAME_SIZE)
      return ParseResult::PARSE_RESULT_OVERFLOW;
    this->data_[this->size_++] = fragment[i];
    if (this->size_ == 2) {
      const size_t expected_size = static_cast<size_t>(this->data_[FRAME_LENGTH_OFFSET]) + 4;
      if (expected_size < FRAME_OVERHEAD || expected_size > MAX_FRAME_SIZE)
        return ParseResult::PARSE_RESULT_INVALID_LENGTH;
      this->expected_size_ = static_cast<uint8_t>(expected_size);
    }
    if (this->complete() && i + 1 < fragment_size)
      return ParseResult::PARSE_RESULT_OVERFLOW;
  }
  return this->complete() ? ParseResult::PARSE_RESULT_OK : ParseResult::PARSE_RESULT_INCOMPLETE;
}

const uint8_t *FrameAssembler::data() const { return this->data_.data(); }

size_t FrameAssembler::size() const { return this->size_; }

bool FrameAssembler::complete() const { return this->expected_size_ != 0 && this->size_ == this->expected_size_; }

}  // namespace esphome::alpha3
