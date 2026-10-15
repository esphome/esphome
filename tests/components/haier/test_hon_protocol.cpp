#include "esphome/components/haier/water_heater/hon_parser.h"
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <vector>
using namespace esphome::haier::water_heater_protocol;
namespace esphome::haier::testing {
std::vector<uint8_t> bytes(const char *hex) {
  std::istringstream stream(hex);
  unsigned byte;
  std::vector<uint8_t> result;
  while (stream >> std::hex >> byte)
    result.push_back(byte);
  return result;
}
unsigned feed(FrameParser &parser, const std::vector<uint8_t> &wire) {
  unsigned count = 0;
  for (auto byte : wire)
    count += parser.feed(byte);
  return count;
}
unsigned feed_head(FrameParser &parser, const std::vector<uint8_t> &wire, size_t count) {
  unsigned frames = 0;
  for (size_t i = 0; i < count; i++)
    frames += parser.feed(wire[i]);
  return frames;
}
TEST(HonProtocolTest, EncodesRequestsAndEscapesBytes) {
  FrameParser parser;
  uint8_t wire[MAX_WIRE_FRAME];
  const uint8_t version[] = {0x00, 0x07};
  const uint8_t query[] = {0x4D, 0x01};
  auto check_tx = [&](FrameType type, const uint8_t *payload, size_t size, const char *expected) {
    const auto count = encode_frame(type, payload, size, wire);
    EXPECT_EQ(std::vector<uint8_t>(wire, wire + count), bytes(expected));
    ASSERT_EQ(feed(parser, std::vector<uint8_t>(wire, wire + count)), 1u);
    EXPECT_EQ(parser.data()[FRAME_FLAGS_OFFSET], 0);
    EXPECT_EQ(parser.data()[FRAME_TYPE_OFFSET], static_cast<uint8_t>(type));
  };
  check_tx(FrameType::FRAME_TYPE_GET_VERSION, version, 2, "FF FF 0A 00 00 00 00 00 00 61 00 07 72");
  check_tx(FrameType::FRAME_TYPE_GET_DEVICE_ID, nullptr, 0, "FF FF 08 00 00 00 00 00 00 70 78");
  check_tx(FrameType::FRAME_TYPE_CONTROL, query, 2, "FF FF 0A 00 00 00 00 00 00 01 4D 01 59");
  check_tx(FrameType::FRAME_TYPE_CONFIRM, nullptr, 0, "FF FF 08 00 00 00 00 00 00 05 0D");
  auto check_control = [&](Command command, const char *expected) {
    uint8_t payload[4];
    encode_control(command, payload);
    check_tx(FrameType::FRAME_TYPE_CONTROL, payload, sizeof(payload), expected);
  };
  check_control({Parameter::PARAMETER_POWER, 1}, "FF FF 0C 00 00 00 00 00 00 01 5D 01 00 01 6C");
  check_control({Parameter::PARAMETER_ECO, 1}, "FF FF 0C 00 00 00 00 00 00 01 5D 05 00 01 70");
  check_control({Parameter::PARAMETER_ECO, 0}, "FF FF 0C 00 00 00 00 00 00 01 5D 05 00 00 6F");
  check_control({Parameter::PARAMETER_BPS, 1}, "FF FF 0C 00 00 00 00 00 00 01 5D 17 00 01 82");
  check_control({Parameter::PARAMETER_TARGET_TEMPERATURE, 6}, "FF FF 0C 00 00 00 00 00 00 01 5D 02 00 06 72");
  const uint8_t network[] = {0, 7, 0, 100};
  check_tx(FrameType::FRAME_TYPE_NETWORK_STATUS, network, 4, "FF FF 0C 00 00 00 00 00 00 F7 00 07 00 64 6E");
  const uint8_t escaped[] = {0xFF, 0x55};
  const size_t escaped_size = encode_frame(FrameType::FRAME_TYPE_CONTROL, escaped, 2, wire);
  EXPECT_EQ(feed(parser, std::vector<uint8_t>(wire, wire + escaped_size)), 1u);
  EXPECT_EQ(encode_frame(FrameType::FRAME_TYPE_CONTROL, escaped, 5, wire), 0u);
}

struct StatusCase {
  const char *name;
  const char *wire;
  uint8_t current_temperature;
  uint8_t target_temperature;
  uint8_t estimated_heating_time;
  bool powered;
  bool heating;
  HonMode mode;
  uint8_t available_water;
};

// Captured from a VH3W on 2026-10-14; the name gives the packet of the session.
static const StatusCase STATUS_CASES[] = {
    {"packet 4",
     "FF FF 3C 00 00 00 00 00 00 06 6D 01 35 05 00 00 00 00 00 00 00 32 19 00 00 35 00 00 00 00 13 00 "
     "2D 13 00 2D 00 00 00 00 00 00 22 00 00 00 00 00 10 10 00 00 00 00 00 02 00 00 00 00 00 00 2E",
     53, 35, 0, false, false, HonMode::HON_MODE_NORMAL, 2},
    {"packet 33",
     "FF FF 3C 00 00 00 00 00 00 06 6D 01 35 05 00 00 00 00 00 00 00 32 19 00 00 35 00 00 00 00 13 00 "
     "2D 13 00 2D 00 00 00 00 00 00 22 00 00 00 00 00 10 11 00 00 00 00 00 02 00 00 00 00 00 00 2F",
     53, 35, 0, true, false, HonMode::HON_MODE_NORMAL, 2},
    {"packet 36",
     "FF FF 3C 00 00 00 00 00 00 06 6D 01 35 05 00 00 00 00 00 00 00 32 19 00 00 35 00 00 00 00 13 00 "
     "2D 13 00 2D 00 00 00 00 00 00 22 00 00 00 00 00 00 01 00 04 00 00 00 02 00 00 00 00 00 00 13",
     53, 35, 0, true, false, HonMode::HON_MODE_ECO, 2},
    {"packet 39",
     "FF FF 3C 00 00 00 00 00 00 06 6D 01 35 32 00 00 00 1D 00 00 00 32 19 00 00 35 00 00 00 00 13 00 "
     "2D 13 00 2D 00 0F 00 00 00 00 22 00 00 00 00 00 00 01 04 01 00 00 00 02 00 00 00 00 00 00 6D",
     53, 80, 29, true, true, HonMode::HON_MODE_BPS, 2},
    {"packet 50",
     "FF FF 3C 00 00 00 00 00 00 06 6D 01 35 05 00 01 00 00 00 00 00 32 19 00 00 35 00 00 00 00 13 00 "
     "2D 13 00 2D 00 00 00 00 00 00 22 00 00 00 00 00 00 01 00 04 00 00 00 02 00 00 00 00 00 00 14",
     53, 35, 0, true, false, HonMode::HON_MODE_ECO, 2},
    {"packet 52",
     "FF FF 3C 00 00 00 00 00 00 06 6D 01 35 32 00 01 00 1D 00 00 00 32 19 00 00 35 00 00 00 00 13 00 "
     "2D 13 00 2D 00 0F 00 00 00 00 22 00 00 00 00 00 00 01 04 01 00 00 00 02 00 00 00 00 00 00 6E",
     53, 80, 29, true, true, HonMode::HON_MODE_BPS, 2},
    {"packet 55",
     "FF FF 3C 00 00 00 00 00 00 06 6D 01 35 05 00 01 00 00 00 00 00 32 19 00 00 35 00 00 00 00 13 00 "
     "2D 13 00 2D 00 00 00 00 00 00 22 00 00 00 00 00 10 11 00 00 00 00 00 02 00 00 00 00 00 00 30",
     53, 35, 0, true, false, HonMode::HON_MODE_NORMAL, 2},
    {"packet 58",
     "FF FF 3C 00 00 00 00 00 00 06 6D 01 35 05 00 01 00 00 00 00 00 32 19 00 00 35 00 00 00 00 13 00 "
     "2D 13 00 2D 00 00 00 00 00 00 22 00 00 00 00 00 10 10 00 00 00 00 00 02 00 00 00 00 00 00 2F",
     53, 35, 0, false, false, HonMode::HON_MODE_NORMAL, 2},
    {"target 35 C",
     "FF FF 3C 00 00 00 00 00 00 06 6D 01 1E 05 00 00 00 06 00 00 00 32 19 00 00 1E 00 00 00 00 13 00 "
     "2D 13 00 2D 00 00 00 00 00 00 22 00 00 00 00 00 10 10 00 00 00 00 00 00 00 00 00 00 00 00 04",
     30, 35, 6, false, false, HonMode::HON_MODE_NORMAL, 0},
    {"target 40 C",
     "FF FF 3C 00 00 00 00 00 00 06 6D 01 1E 0A 00 01 00 0B 00 00 00 32 19 00 00 1E 00 00 00 00 13 00 "
     "2D 13 00 2D 00 0F 00 00 00 00 22 00 00 00 00 00 10 11 00 01 00 00 00 00 00 00 00 00 00 00 20",
     30, 40, 11, true, true, HonMode::HON_MODE_NORMAL, 0},
    {"target 75 C",
     "FF FF 3C 00 00 00 00 00 00 06 6D 01 1F 2D 00 01 00 2F 00 00 00 32 19 00 00 1F 00 00 00 00 13 00 "
     "2D 13 00 2D 00 0F 00 00 00 00 22 00 00 00 00 00 10 11 80 01 00 00 00 00 00 00 00 00 00 00 E9",
     31, 75, 47, true, true, HonMode::HON_MODE_NORMAL, 0},
    {"target back to 35 C",
     "FF FF 3C 00 00 00 00 00 00 06 6D 01 20 05 00 01 00 04 00 00 00 32 19 00 00 20 00 00 00 00 13 00 "
     "2D 13 00 2D 00 0F 00 00 00 00 22 00 00 00 00 00 10 11 00 01 00 00 00 00 00 00 00 00 00 00 18",
     32, 35, 4, true, true, HonMode::HON_MODE_NORMAL, 0},
};

TEST(HonProtocolTest, DecodesCapturedStatusFrames) {
  FrameParser parser;
  for (const auto &expected : STATUS_CASES) {
    SCOPED_TRACE(expected.name);
    Status status{};
    ASSERT_EQ(feed(parser, bytes(expected.wire)), 1u);
    ASSERT_TRUE(decode_status(parser.data(), parser.size(), status));
    EXPECT_EQ(status.current_temperature, expected.current_temperature);
    EXPECT_EQ(status.target_temperature, expected.target_temperature);
    EXPECT_EQ(status.estimated_heating_time, expected.estimated_heating_time);
    EXPECT_EQ(status.powered, expected.powered);
    EXPECT_EQ(status.heating, expected.heating);
    EXPECT_EQ(status.mode, expected.mode);
    EXPECT_EQ(status.available_water, expected.available_water);
  }
}

TEST(HonProtocolTest, ConfirmsCommandsAgainstStatus) {
  Status status{};
  status.powered = true;
  status.target_temperature = 40;
  status.mode = HonMode::HON_MODE_ECO;
  EXPECT_TRUE(command_confirmed({Parameter::PARAMETER_POWER, 1}, status));
  EXPECT_FALSE(command_confirmed({Parameter::PARAMETER_POWER, 0}, status));
  EXPECT_TRUE(command_confirmed({Parameter::PARAMETER_TARGET_TEMPERATURE, 10}, status));
  EXPECT_FALSE(command_confirmed({Parameter::PARAMETER_TARGET_TEMPERATURE, 11}, status));
  EXPECT_TRUE(command_confirmed({Parameter::PARAMETER_ECO, 1}, status));
  EXPECT_FALSE(command_confirmed({Parameter::PARAMETER_ECO, 0}, status));
  EXPECT_FALSE(command_confirmed({Parameter::PARAMETER_BPS, 1}, status));
}

TEST(HonProtocolTest, KeepsTemperatureZonesIndependent) {
  Status status{};
  uint8_t independent[61] = {};
  independent[0] = STATUS_FRAME_LENGTH;
  independent[FRAME_TYPE_OFFSET] = static_cast<uint8_t>(FrameType::FRAME_TYPE_STATUS);
  independent[8] = 0x6D;
  independent[9] = 1;
  independent[10] = 42;
  independent[11] = 5;
  independent[23] = 43;
  ASSERT_TRUE(decode_status(independent, sizeof(independent), status));
  EXPECT_EQ(status.current_temperature, 42);
  EXPECT_EQ(status.secondary_temperature, 43);
}

TEST(HonProtocolTest, RejectsCorruptionAndResynchronizes) {
  FrameParser parser;
  Status status{};
  auto valid = bytes("FF FF 3C 00 00 00 00 00 00 06 6D 01 35 32 00 00 00 1D 00 00 00 32 19 00 00 35 00 00 00 00 13 00 "
                     "2D 13 00 2D 00 0F 00 00 00 00 22 00 00 00 00 00 00 01 04 01 00 00 00 02 00 00 00 00 00 00 6D");
  auto damaged = valid;
  damaged.back() ^= 1;
  EXPECT_EQ(feed(parser, damaged), 0u);
  EXPECT_EQ(feed(parser, valid), 1u);
  parser.reset();
  EXPECT_EQ(feed_head(parser, valid, 20), 0u);
  parser.reset();  // Partial frame timeout.
  EXPECT_EQ(feed(parser, valid), 1u);
  parser.reset();
  EXPECT_EQ(feed_head(parser, valid, 20), 0u);
  EXPECT_EQ(feed(parser, valid), 1u);  // New separator resynchronizes.
  EXPECT_EQ(feed(parser, bytes("FF FF 07 7F FF 55 DA")), 0u);
  EXPECT_EQ(feed(parser, valid), 1u);
  auto crc = bytes("FF FF 0D 40 00 00 00 00 00 01 4D 01 FF 55 BB FF 55 FF 55 D1 3C");
  EXPECT_EQ(feed(parser, crc), 1u);
  EXPECT_FALSE(decode_status(parser.data(), parser.size(), status));
  crc.back() ^= 1;
  EXPECT_EQ(feed(parser, crc), 0u);
  EXPECT_EQ(feed(parser, bytes("FF FF 08 00 00 00 00 00 00 05 0D")), 1u);
  EXPECT_FALSE(decode_status(parser.data(), 0, status));
}

TEST(HonProtocolTest, ValidatesFramesLongerThanTheStoredHead) {
  FrameParser parser;
  std::vector<uint8_t> frame(MAX_FRAME_LENGTH + 1, 0);
  frame[0] = MAX_FRAME_LENGTH;
  frame[FRAME_TYPE_OFFSET] = static_cast<uint8_t>(FrameType::FRAME_TYPE_VERSION);
  frame[MAX_FRAME_LENGTH - 1] = 0x7E;  // a byte past the stored head still counts
  uint8_t checksum = 0;
  for (size_t i = 0; i < MAX_FRAME_LENGTH; i++)
    checksum += frame[i];
  frame[MAX_FRAME_LENGTH] = checksum;
  std::vector<uint8_t> wire = {0xFF, 0xFF};
  wire.insert(wire.end(), frame.begin(), frame.end());
  EXPECT_EQ(feed(parser, wire), 1u);
  EXPECT_EQ(parser.size(), MAX_FRAME_LENGTH + 1u);
  EXPECT_EQ(parser.stored_size(), MAX_STORED_FRAME);
  EXPECT_EQ(parser.data()[FRAME_TYPE_OFFSET], static_cast<uint8_t>(FrameType::FRAME_TYPE_VERSION));
  wire.back() ^= 1;
  EXPECT_EQ(feed(parser, wire), 0u);
  EXPECT_TRUE(parser.complete());
}
}  // namespace esphome::haier::testing
