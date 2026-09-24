#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace esphome::alpha3 {

constexpr uint8_t GENI_REQUEST_START = 0x27;
constexpr uint8_t GENI_RESPONSE_START = 0x24;
constexpr uint8_t GENI_LEGACY_UNIT_ADDRESS = 0xE7;
constexpr uint8_t GENI_BROADCAST_ADDRESS = 0xFF;
constexpr uint8_t GENI_REQUEST_SOURCE = 0xF8;
constexpr uint8_t GENI_RESPONSE_DESTINATION = 0xF8;
constexpr uint8_t GENI_PARAMETER_CLASS = 0x02;
constexpr uint8_t GENI_DISCOVERY_CLASS = 0x04;
constexpr uint8_t GENI_OBJECT_CLASS = 0x0A;
constexpr size_t MAX_FRAME_SIZE = 96;
constexpr size_t BLE_FRAGMENT_SIZE = 20;
constexpr uint32_t RESPONSE_TIMEOUT_MS = 1000;
constexpr float PASCALS_PER_METER = 9804.0F;

enum class GeniOperation : uint8_t {
  GENI_OPERATION_GET = 0,
  GENI_OPERATION_SET = 2,
};

enum class ParseResult : uint8_t {
  PARSE_RESULT_OK,
  PARSE_RESULT_INCOMPLETE,
  PARSE_RESULT_OVERFLOW,
  PARSE_RESULT_INVALID_START,
  PARSE_RESULT_INVALID_LENGTH,
  PARSE_RESULT_INVALID_ADDRESS,
  PARSE_RESULT_INVALID_OPERATION,
  PARSE_RESULT_INVALID_APDU_LENGTH,
  PARSE_RESULT_INVALID_CRC,
  PARSE_RESULT_INVALID_ACK,
  PARSE_RESULT_UNEXPECTED_CLASS,
  PARSE_RESULT_UNEXPECTED_TYPE,
  PARSE_RESULT_UNEXPECTED_VERSION,
  PARSE_RESULT_INVALID_PAYLOAD_LENGTH,
};

struct ObjectAddress {
  uint8_t data_id;
  uint16_t sub_id;
};

struct ObjectVersionSpec {
  uint8_t version;
  uint8_t payload_size;
  bool allow_longer;
};

struct EncodedFrame {
  std::array<uint8_t, MAX_FRAME_SIZE> data{};
  uint8_t size{0};
};

struct ParsedFrame {
  uint8_t data_class{0};
  GeniOperation operation{GeniOperation::GENI_OPERATION_GET};
  const uint8_t *data{nullptr};
  uint8_t data_size{0};
};

struct ParsedObject {
  uint16_t type{0};
  uint8_t version{0};
  const uint8_t *payload{nullptr};
  uint32_t payload_size{0};
};

uint16_t geni_crc16(const uint8_t *data, size_t size);
bool build_discovery_get(EncodedFrame &output);
bool build_parameter_get(uint8_t destination, uint8_t parameter_id, EncodedFrame &output);
bool build_object_get(uint8_t destination, ObjectAddress address, EncodedFrame &output);
bool build_object_set(uint8_t destination, ObjectAddress address, uint16_t type, uint8_t version,
                      const uint8_t *payload, size_t payload_size, EncodedFrame &output);
ParseResult parse_discovery_response(const uint8_t *data, size_t size, uint8_t &unit_address);
ParseResult parse_response_frame(const uint8_t *data, size_t size, uint8_t expected_source, ParsedFrame &output);
ParseResult parse_object_response(const ParsedFrame &frame, uint16_t expected_type, const ObjectVersionSpec *versions,
                                  size_t version_count, ParsedObject &output);
ParseResult parse_write_ack(const ParsedFrame &frame);

class FrameAssembler {
 public:
  void reset();
  ParseResult append(const uint8_t *fragment, size_t fragment_size);
  const uint8_t *data() const;
  size_t size() const;
  bool complete() const;

 protected:
  std::array<uint8_t, MAX_FRAME_SIZE> data_{};
  uint8_t size_{0};
  uint8_t expected_size_{0};
};

}  // namespace esphome::alpha3
