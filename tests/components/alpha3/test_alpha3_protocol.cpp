#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "esphome/components/alpha3/alpha3_protocol.h"

namespace esphome::alpha3::testing {
namespace {

constexpr std::array<uint8_t, 39> FLOW_RESPONSE{{
    0x24, 0x23, 0xF8, 0xE7, 0x0A, 0x1F, 0x00, 0x01, 0x30, 0x01, 0x00, 0x00, 0x18,
    0x38, 0x92, 0xC4, 0x2D, 0x47, 0x16, 0x5F, 0xF5, 0x7F, 0xFF, 0xFF, 0xFF, 0x7F,
    0xFF, 0xFF, 0xFF, 0x41, 0x77, 0xFE, 0x0A, 0x7F, 0xFF, 0xFF, 0xFF, 0x1A, 0x21,
}};

constexpr std::array<uint8_t, 9> WRITE_ACK_GET_FORM{{0x24, 0x05, 0xF8, 0xE7, 0x0A, 0x01, 0x00, 0xAE, 0xA2}};
constexpr std::array<uint8_t, 9> WRITE_ACK_SET_FORM{{0x24, 0x05, 0xF8, 0xE7, 0x0A, 0x81, 0x00, 0xB5, 0x3A}};

constexpr std::array<uint8_t, 39> DISCOVERED_FLOW_RESPONSE{{
    0x24, 0x23, 0xF8, 0xE6, 0x0A, 0x1F, 0x00, 0x01, 0x30, 0x01, 0x00, 0x00, 0x18,
    0x38, 0xA7, 0xC6, 0xB4, 0x45, 0xEE, 0x92, 0x7C, 0x7F, 0xFF, 0xFF, 0xFF, 0x7F,
    0xFF, 0xFF, 0xFF, 0x41, 0xEF, 0x84, 0x65, 0x7F, 0xFF, 0xFF, 0xFF, 0x8C, 0x9C,
}};

constexpr std::array<uint8_t, 19> DISCOVERY_RESPONSE{{
    0x24, 0x0F, 0xF8, 0xE6, 0x04, 0x02, 0xE6, 0xF7, 0x02, 0x03, 0x34, 0x01, 0x00, 0x00, 0x02, 0xFF, 0x58, 0x5B, 0x24,
}};

template<typename... Bytes> void expect_frame(bool built, const EncodedFrame &actual, Bytes... bytes) {
  const std::array<uint8_t, sizeof...(bytes)> expected{{static_cast<uint8_t>(bytes)...}};
  EXPECT_TRUE(built);
  ASSERT_EQ(actual.size, expected.size());
  for (size_t i = 0; i < expected.size(); i++)
    EXPECT_EQ(actual.data[i], expected[i]) << "byte " << i;
}

#define EXPECT_FRAME(expression, ...) expect_frame((expression), actual, __VA_ARGS__)

uint16_t fixture_crc16(const uint8_t *data, size_t size) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < size; i++) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; bit++)
      crc = (crc & 0x8000U) != 0 ? static_cast<uint16_t>((crc << 1) ^ 0x1021U) : static_cast<uint16_t>(crc << 1);
  }
  return static_cast<uint16_t>(~crc);
}

template<size_t N> void update_fixture_crc(std::array<uint8_t, N> &frame) {
  const uint16_t crc = fixture_crc16(frame.data() + 1, N - 3);
  frame[N - 2] = static_cast<uint8_t>(crc >> 8);
  frame[N - 1] = static_cast<uint8_t>(crc);
}

template<size_t N> ParseResult parse_flow(const std::array<uint8_t, N> &frame, ParsedFrame &parsed) {
  return parse_response_frame(frame.data(), frame.size(), 0xE7, parsed);
}

constexpr ObjectVersionSpec FLOW_VERSION[]{{1, 24, false}};
constexpr ObjectVersionSpec ENERGY_VERSION[]{{1, 8, true}};

}  // namespace

TEST(Alpha3ProtocolBuild, BuildsKnownGetFramesWithLiteralCrcs) {
  EncodedFrame actual;
  EXPECT_FRAME(build_object_get(0xE7, {93, 289}, actual), 0x27, 0x07, 0xE7, 0xF8, 0x0A, 0x03, 0x5D, 0x01, 0x21, 0x52,
               0x1F);
  EXPECT_FRAME(build_object_get(0xE7, {87, 69}, actual), 0x27, 0x07, 0xE7, 0xF8, 0x0A, 0x03, 0x57, 0x00, 0x45, 0x8A,
               0xCD);
  EXPECT_FRAME(build_parameter_get(0xE7, 148, actual), 0x27, 0x05, 0xE7, 0xF8, 0x02, 0x01, 0x94, 0x6A, 0xD4);
  EXPECT_FRAME(build_parameter_get(0xE7, 149, actual), 0x27, 0x05, 0xE7, 0xF8, 0x02, 0x01, 0x95, 0x7A, 0xF5);
  EXPECT_FRAME(build_parameter_get(0xE7, 150, actual), 0x27, 0x05, 0xE7, 0xF8, 0x02, 0x01, 0x96, 0x4A, 0x96);
  EXPECT_FRAME(build_object_get(0xE7, {93, 1}, actual), 0x27, 0x07, 0xE7, 0xF8, 0x0A, 0x03, 0x5D, 0x00, 0x01, 0x45,
               0x4C);
  EXPECT_FRAME(build_object_get(0xE7, {87, 1}, actual), 0x27, 0x07, 0xE7, 0xF8, 0x0A, 0x03, 0x57, 0x00, 0x01, 0x82,
               0x8D);
  EXPECT_FRAME(build_parameter_get(0xE7, 156, actual), 0x27, 0x05, 0xE7, 0xF8, 0x02, 0x01, 0x9C, 0xEB, 0xDC);
  EXPECT_FRAME(build_parameter_get(0xE7, 158, actual), 0x27, 0x05, 0xE7, 0xF8, 0x02, 0x01, 0x9E, 0xCB, 0x9E);
}

TEST(Alpha3ProtocolBuild, CalculatesCrcOverLengthAndBody) {
  constexpr std::array<uint8_t, 9> frame{{0x27, 0x05, 0xE7, 0xF8, 0x02, 0x01, 0x94, 0x6A, 0xD4}};
  EXPECT_EQ(geni_crc16(frame.data() + 1, frame.size() - 3), 0x6AD4);
}

TEST(Alpha3ProtocolBuild, AddressesReadsAndWritesToTheDiscoveredUnitWithLiteralCrcs) {
  EncodedFrame actual;
  EXPECT_FRAME(build_object_get(0xE6, {93, 289}, actual), 0x27, 0x07, 0xE6, 0xF8, 0x0A, 0x03, 0x5D, 0x01, 0x21, 0xEA,
               0x7E);
  EXPECT_FRAME(build_parameter_get(0xE6, 148, actual), 0x27, 0x05, 0xE6, 0xF8, 0x02, 0x01, 0x94, 0xC0, 0x85);
  constexpr std::array<uint8_t, 4> payload{{0x00, 0x03, 0x00, 0x00}};
  EXPECT_FRAME(build_object_set(0xE6, {86, 100}, 340, 1, payload.data(), payload.size(), actual), 0x27, 0x11, 0xE6,
               0xF8, 0x0A, 0x8D, 0x56, 0x00, 0x64, 0x01, 0x54, 0x01, 0x00, 0x00, 0x04, 0x00, 0x03, 0x00, 0x00, 0xDA,
               0xB8);
}

TEST(Alpha3ProtocolDiscovery, BuildsExactBroadcastRequest) {
  EncodedFrame actual;
  EXPECT_FRAME(build_discovery_get(actual), 0x27, 0x0F, 0xFF, 0xF8, 0x04, 0x02, 0x2E, 0x2F, 0x02, 0x03, 0x94, 0x95,
               0x96, 0x00, 0x02, 0x02, 0x03, 0x4A, 0x4C);
}

TEST(Alpha3ProtocolDiscovery, ExtractsSourceFromExactDiscoveryResponse) {
  uint8_t address = GENI_BROADCAST_ADDRESS;
  ASSERT_EQ(parse_discovery_response(DISCOVERY_RESPONSE.data(), DISCOVERY_RESPONSE.size(), address),
            ParseResult::PARSE_RESULT_OK);
  EXPECT_EQ(address, 0xE6);

  auto other_unit = DISCOVERY_RESPONSE;
  other_unit[3] = 0xE5;
  other_unit[6] = 0xE5;
  update_fixture_crc(other_unit);
  ASSERT_EQ(parse_discovery_response(other_unit.data(), other_unit.size(), address), ParseResult::PARSE_RESULT_OK);
  EXPECT_EQ(address, 0xE5);
}

TEST(Alpha3ProtocolDiscovery, ResolvesAddressWhenTheEmbeddedIdentityFieldsVary) {
  for (const auto [offset, value] : std::array<std::pair<size_t, uint8_t>, 3>{{{10, 53}, {11, 2}, {12, 1}}}) {
    auto changed_identity = DISCOVERY_RESPONSE;
    changed_identity[offset] = value;
    update_fixture_crc(changed_identity);

    uint8_t address = GENI_BROADCAST_ADDRESS;
    ASSERT_EQ(parse_discovery_response(changed_identity.data(), changed_identity.size(), address),
              ParseResult::PARSE_RESULT_OK);
    EXPECT_EQ(address, 0xE6);
  }
}

TEST(Alpha3ProtocolDiscovery, RejectsIncompleteExtraCorruptAndUnexpectedDiscoveryBytes) {
  uint8_t address = GENI_BROADCAST_ADDRESS;
  for (size_t size = 0; size < DISCOVERY_RESPONSE.size(); size++) {
    EXPECT_EQ(parse_discovery_response(DISCOVERY_RESPONSE.data(), size, address), ParseResult::PARSE_RESULT_INCOMPLETE);
    EXPECT_EQ(address, GENI_BROADCAST_ADDRESS);
  }
  EXPECT_EQ(parse_discovery_response(nullptr, DISCOVERY_RESPONSE.size(), address),
            ParseResult::PARSE_RESULT_INCOMPLETE);
  std::array<uint8_t, 20> trailing_byte{};
  std::copy(DISCOVERY_RESPONSE.begin(), DISCOVERY_RESPONSE.end(), trailing_byte.begin());
  EXPECT_EQ(parse_discovery_response(trailing_byte.data(), trailing_byte.size(), address),
            ParseResult::PARSE_RESULT_INVALID_LENGTH);

  auto bad_crc = DISCOVERY_RESPONSE;
  bad_crc.back() ^= 1;
  EXPECT_EQ(parse_discovery_response(bad_crc.data(), bad_crc.size(), address), ParseResult::PARSE_RESULT_INVALID_CRC);
  for (size_t offset = 0; offset < DISCOVERY_RESPONSE.size() - 2; offset++) {
    if (offset >= 10 && offset <= 12)
      continue;
    auto malformed = DISCOVERY_RESPONSE;
    malformed[offset] ^= 1;
    update_fixture_crc(malformed);
    EXPECT_NE(parse_discovery_response(malformed.data(), malformed.size(), address), ParseResult::PARSE_RESULT_OK)
        << "offset " << offset;
    EXPECT_EQ(address, GENI_BROADCAST_ADDRESS) << "offset " << offset;
  }
  auto broadcast_source = DISCOVERY_RESPONSE;
  broadcast_source[3] = 0xFF;
  broadcast_source[6] = 0xFF;
  update_fixture_crc(broadcast_source);
  EXPECT_EQ(parse_discovery_response(broadcast_source.data(), broadcast_source.size(), address),
            ParseResult::PARSE_RESULT_INVALID_ADDRESS);
  for (uint8_t operation : {0x42, 0x82, 0xC2}) {
    auto not_get = DISCOVERY_RESPONSE;
    not_get[5] = operation;
    update_fixture_crc(not_get);
    EXPECT_EQ(parse_discovery_response(not_get.data(), not_get.size(), address),
              ParseResult::PARSE_RESULT_INVALID_OPERATION);
  }
}

TEST(Alpha3ProtocolParse, RequiresTheCurrentConnectionSourceForNormalResponses) {
  ParsedFrame parsed;
  EXPECT_EQ(parse_response_frame(DISCOVERED_FLOW_RESPONSE.data(), DISCOVERED_FLOW_RESPONSE.size(), 0xE6, parsed),
            ParseResult::PARSE_RESULT_OK);
  EXPECT_EQ(parse_response_frame(DISCOVERED_FLOW_RESPONSE.data(), DISCOVERED_FLOW_RESPONSE.size(), 0xE7, parsed),
            ParseResult::PARSE_RESULT_INVALID_ADDRESS);
  EXPECT_EQ(parsed.data, nullptr);
  EXPECT_EQ(parse_response_frame(FLOW_RESPONSE.data(), FLOW_RESPONSE.size(), 0xE6, parsed),
            ParseResult::PARSE_RESULT_INVALID_ADDRESS);
  EXPECT_EQ(parse_response_frame(FLOW_RESPONSE.data(), FLOW_RESPONSE.size(), 0xE7, parsed),
            ParseResult::PARSE_RESULT_OK);
}

TEST(Alpha3ProtocolBuild, BuildsObjectSetAndRejectsUnencodablePayloads) {
  constexpr std::array<uint8_t, 4> operation_payload{{0x00, 0x03, 0x00, 0x00}};
  EncodedFrame actual;
  EXPECT_FRAME(build_object_set(0xE7, {86, 100}, 340, 1, operation_payload.data(), operation_payload.size(), actual),
               0x27, 0x11, 0xE7, 0xF8, 0x0A, 0x8D, 0x56, 0x00, 0x64, 0x01, 0x54, 0x01, 0x00, 0x00, 0x04, 0x00, 0x03,
               0x00, 0x00, 0xCA, 0x5A);

  EXPECT_FALSE(build_object_set(0xE7, {86, 100}, 340, 1, nullptr, 1, actual));
  EXPECT_EQ(actual.size, 0);
  std::array<uint8_t, 55> oversized_payload{};
  EXPECT_FALSE(build_object_set(0xE7, {86, 100}, 340, 1, oversized_payload.data(), oversized_payload.size(), actual));
  EXPECT_EQ(actual.size, 0);
}

TEST(Alpha3ProtocolParse, ParsesCompleteFlowResponse) {
  ParsedFrame parsed;
  ASSERT_EQ(parse_flow(FLOW_RESPONSE, parsed), ParseResult::PARSE_RESULT_OK);
  EXPECT_EQ(parsed.data_class, 0x0A);
  EXPECT_EQ(parsed.operation, GeniOperation::GENI_OPERATION_GET);
  ASSERT_EQ(parsed.data_size, 31);
  EXPECT_EQ(parsed.data[0], 0x00);
  EXPECT_EQ(parsed.data[30], 0xFF);
}

TEST(Alpha3ProtocolParse, AcceptsHardwareResponseAfterTwentyAndNineteenByteFragments) {
  FrameAssembler assembler;
  ASSERT_EQ(assembler.append(DISCOVERED_FLOW_RESPONSE.data(), 20), ParseResult::PARSE_RESULT_INCOMPLETE);
  ASSERT_EQ(assembler.append(DISCOVERED_FLOW_RESPONSE.data() + 20, 19), ParseResult::PARSE_RESULT_OK);
  ASSERT_EQ(assembler.size(), 39U);
  EXPECT_EQ(geni_crc16(assembler.data() + 1, assembler.size() - 3), 0x8C9C);
  ParsedFrame parsed;
  ASSERT_EQ(parse_response_frame(assembler.data(), assembler.size(), 0xE6, parsed), ParseResult::PARSE_RESULT_OK);
  ParsedObject object;
  EXPECT_EQ(parse_object_response(parsed, 304, FLOW_VERSION, std::size(FLOW_VERSION), object),
            ParseResult::PARSE_RESULT_OK);
}

TEST(Alpha3ProtocolParse, RejectsMalformedFrameBoundariesAndHeaderFields) {
  ParsedFrame parsed;
  EXPECT_EQ(parse_response_frame(FLOW_RESPONSE.data(), FLOW_RESPONSE.size() - 1, 0xE7, parsed),
            ParseResult::PARSE_RESULT_INCOMPLETE);

  auto invalid_start = FLOW_RESPONSE;
  invalid_start[0] = 0x25;
  EXPECT_EQ(parse_flow(invalid_start, parsed), ParseResult::PARSE_RESULT_INVALID_START);

  auto invalid_length = FLOW_RESPONSE;
  invalid_length[1] = 0x60;
  EXPECT_EQ(parse_flow(invalid_length, parsed), ParseResult::PARSE_RESULT_INVALID_LENGTH);

  auto invalid_destination = FLOW_RESPONSE;
  invalid_destination[2] = 0xE7;
  update_fixture_crc(invalid_destination);
  EXPECT_EQ(parse_flow(invalid_destination, parsed), ParseResult::PARSE_RESULT_INVALID_ADDRESS);

  auto invalid_source = FLOW_RESPONSE;
  invalid_source[3] = 0xF8;
  update_fixture_crc(invalid_source);
  EXPECT_EQ(parse_flow(invalid_source, parsed), ParseResult::PARSE_RESULT_INVALID_ADDRESS);

  auto invalid_operation = FLOW_RESPONSE;
  invalid_operation[5] = 0x5F;
  update_fixture_crc(invalid_operation);
  EXPECT_EQ(parse_flow(invalid_operation, parsed), ParseResult::PARSE_RESULT_INVALID_OPERATION);

  auto invalid_apdu_length = FLOW_RESPONSE;
  invalid_apdu_length[5] = 0x1E;
  update_fixture_crc(invalid_apdu_length);
  EXPECT_EQ(parse_flow(invalid_apdu_length, parsed), ParseResult::PARSE_RESULT_INVALID_APDU_LENGTH);

  auto invalid_crc = FLOW_RESPONSE;
  invalid_crc.back() ^= 0x01;
  EXPECT_EQ(parse_flow(invalid_crc, parsed), ParseResult::PARSE_RESULT_INVALID_CRC);
}

TEST(Alpha3ProtocolObjectParse, ValidatesObjectResponseAgainstExpectedSchema) {
  ParsedFrame parsed;
  ASSERT_EQ(parse_flow(FLOW_RESPONSE, parsed), ParseResult::PARSE_RESULT_OK);
  ParsedObject object;
  ASSERT_EQ(parse_object_response(parsed, 304, FLOW_VERSION, std::size(FLOW_VERSION), object),
            ParseResult::PARSE_RESULT_OK);
  EXPECT_EQ(object.type, 304);
  EXPECT_EQ(object.version, 1);
  ASSERT_EQ(object.payload_size, 24U);
  EXPECT_EQ(object.payload[0], 0x38);

  auto nonzero_ack = FLOW_RESPONSE;
  nonzero_ack[6] = 1;
  update_fixture_crc(nonzero_ack);
  ASSERT_EQ(parse_flow(nonzero_ack, parsed), ParseResult::PARSE_RESULT_OK);
  EXPECT_EQ(parse_object_response(parsed, 304, FLOW_VERSION, std::size(FLOW_VERSION), object),
            ParseResult::PARSE_RESULT_INVALID_ACK);

  auto wrong_class = FLOW_RESPONSE;
  wrong_class[4] = 0x02;
  update_fixture_crc(wrong_class);
  ASSERT_EQ(parse_flow(wrong_class, parsed), ParseResult::PARSE_RESULT_OK);
  EXPECT_EQ(parse_object_response(parsed, 304, FLOW_VERSION, std::size(FLOW_VERSION), object),
            ParseResult::PARSE_RESULT_UNEXPECTED_CLASS);

  auto wrong_type = FLOW_RESPONSE;
  wrong_type[7] = 0x01;
  wrong_type[8] = 0x31;
  update_fixture_crc(wrong_type);
  ASSERT_EQ(parse_flow(wrong_type, parsed), ParseResult::PARSE_RESULT_OK);
  EXPECT_EQ(parse_object_response(parsed, 304, FLOW_VERSION, std::size(FLOW_VERSION), object),
            ParseResult::PARSE_RESULT_UNEXPECTED_TYPE);

  auto wrong_version = FLOW_RESPONSE;
  wrong_version[9] = 2;
  update_fixture_crc(wrong_version);
  ASSERT_EQ(parse_flow(wrong_version, parsed), ParseResult::PARSE_RESULT_OK);
  EXPECT_EQ(parse_object_response(parsed, 304, FLOW_VERSION, std::size(FLOW_VERSION), object),
            ParseResult::PARSE_RESULT_UNEXPECTED_VERSION);

  std::array<uint8_t, 38> short_payload{};
  for (size_t i = 0; i < short_payload.size() - 2; i++)
    short_payload[i] = FLOW_RESPONSE[i];
  short_payload[1] = 0x22;
  short_payload[5] = 0x1E;
  short_payload[12] = 23;
  update_fixture_crc(short_payload);
  ASSERT_EQ(parse_flow(short_payload, parsed), ParseResult::PARSE_RESULT_OK);
  EXPECT_EQ(parse_object_response(parsed, 304, FLOW_VERSION, std::size(FLOW_VERSION), object),
            ParseResult::PARSE_RESULT_INVALID_PAYLOAD_LENGTH);
}

TEST(Alpha3ProtocolObjectParse, AppliesExactAndPrefixPayloadPolicies) {
  std::array<uint8_t, 40> longer_flow{};
  for (size_t i = 0; i < FLOW_RESPONSE.size() - 2; i++)
    longer_flow[i] = FLOW_RESPONSE[i];
  longer_flow[1] = 0x24;
  longer_flow[5] = 0x20;
  longer_flow[12] = 25;
  longer_flow[37] = 0x00;
  update_fixture_crc(longer_flow);
  ParsedFrame parsed;
  ASSERT_EQ(parse_flow(longer_flow, parsed), ParseResult::PARSE_RESULT_OK);
  ParsedObject object;
  EXPECT_EQ(parse_object_response(parsed, 304, FLOW_VERSION, std::size(FLOW_VERSION), object),
            ParseResult::PARSE_RESULT_INVALID_PAYLOAD_LENGTH);

  auto energy = FLOW_RESPONSE;
  energy[7] = 0x00;
  energy[8] = 0xE8;
  energy[9] = 0x01;
  energy[10] = 0x00;
  energy[11] = 0x00;
  energy[12] = 24;
  update_fixture_crc(energy);
  ASSERT_EQ(parse_flow(energy, parsed), ParseResult::PARSE_RESULT_OK);
  EXPECT_EQ(parse_object_response(parsed, 232, ENERGY_VERSION, std::size(ENERGY_VERSION), object),
            ParseResult::PARSE_RESULT_OK);
  EXPECT_EQ(object.payload_size, 24U);
}

TEST(Alpha3ProtocolAckParse, AcceptsBothDocumentedWriteAckForms) {
  ParsedFrame parsed;
  ASSERT_EQ(parse_response_frame(WRITE_ACK_GET_FORM.data(), WRITE_ACK_GET_FORM.size(), 0xE7, parsed),
            ParseResult::PARSE_RESULT_OK);
  EXPECT_EQ(parse_write_ack(parsed), ParseResult::PARSE_RESULT_OK);
  ASSERT_EQ(parse_response_frame(WRITE_ACK_SET_FORM.data(), WRITE_ACK_SET_FORM.size(), 0xE7, parsed),
            ParseResult::PARSE_RESULT_OK);
  EXPECT_EQ(parse_write_ack(parsed), ParseResult::PARSE_RESULT_OK);
}

TEST(Alpha3ProtocolAssembler, ReassemblesWholeAndFragmentedNotifications) {
  FrameAssembler assembler;
  EXPECT_EQ(assembler.append(FLOW_RESPONSE.data(), FLOW_RESPONSE.size()), ParseResult::PARSE_RESULT_OK);
  EXPECT_TRUE(assembler.complete());
  EXPECT_EQ(assembler.size(), FLOW_RESPONSE[1] + 4U);

  assembler.reset();
  EXPECT_EQ(assembler.append(FLOW_RESPONSE.data(), 20), ParseResult::PARSE_RESULT_INCOMPLETE);
  EXPECT_FALSE(assembler.complete());
  EXPECT_EQ(assembler.append(FLOW_RESPONSE.data() + 20, FLOW_RESPONSE.size() - 20), ParseResult::PARSE_RESULT_OK);
  EXPECT_TRUE(assembler.complete());
  EXPECT_EQ(assembler.size(), FLOW_RESPONSE[1] + 4U);

  assembler.reset();
  for (size_t i = 0; i < FLOW_RESPONSE.size(); i++) {
    const ParseResult expected =
        i + 1 == FLOW_RESPONSE.size() ? ParseResult::PARSE_RESULT_OK : ParseResult::PARSE_RESULT_INCOMPLETE;
    EXPECT_EQ(assembler.append(FLOW_RESPONSE.data() + i, 1), expected);
    EXPECT_EQ(assembler.complete(), i + 1 == FLOW_RESPONSE.size());
  }
}

TEST(Alpha3ProtocolAssembler, RejectsInvalidLengthsOverflowAndSecondFrame) {
  FrameAssembler assembler;
  constexpr std::array<uint8_t, 2> too_short{{0x24, 0x03}};
  EXPECT_EQ(assembler.append(too_short.data(), too_short.size()), ParseResult::PARSE_RESULT_INVALID_LENGTH);

  assembler.reset();
  constexpr std::array<uint8_t, 2> too_long{{0x24, 0x60}};
  EXPECT_EQ(assembler.append(too_long.data(), too_long.size()), ParseResult::PARSE_RESULT_INVALID_LENGTH);

  assembler.reset();
  std::array<uint8_t, MAX_FRAME_SIZE> bytes{};
  EXPECT_EQ(assembler.append(bytes.data(), bytes.size()), ParseResult::PARSE_RESULT_INVALID_LENGTH);

  assembler.reset();
  std::array<uint8_t, MAX_FRAME_SIZE + 1> maximum_frame_with_trailer{};
  maximum_frame_with_trailer[0] = GENI_RESPONSE_START;
  maximum_frame_with_trailer[1] = MAX_FRAME_SIZE - 4;
  EXPECT_EQ(assembler.append(maximum_frame_with_trailer.data(), maximum_frame_with_trailer.size()),
            ParseResult::PARSE_RESULT_OVERFLOW);
  EXPECT_TRUE(assembler.complete());
  EXPECT_EQ(assembler.size(), MAX_FRAME_SIZE);

  assembler.reset();
  ASSERT_EQ(assembler.append(FLOW_RESPONSE.data(), FLOW_RESPONSE.size()), ParseResult::PARSE_RESULT_OK);
  EXPECT_EQ(assembler.append(FLOW_RESPONSE.data(), 1), ParseResult::PARSE_RESULT_OVERFLOW);
  EXPECT_TRUE(assembler.complete());
}

}  // namespace esphome::alpha3::testing
