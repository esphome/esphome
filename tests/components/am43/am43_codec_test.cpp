#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/am43/am43_base.h"

namespace esphome::am43::testing {

static std::vector<uint8_t> bytes(const Am43Packet &packet) {
  return std::vector<uint8_t>(packet.data, packet.data + packet.length);
}
// The requests are returned by pointer or by value
static std::vector<uint8_t> bytes(const Am43Packet *packet) { return bytes(*packet); }

TEST(Am43Encoder, RequestsMatchTheDeviceFraming) {
  Am43Encoder encoder;
  // 00 ff 00 00 9a, command, length, data, then the inverted XOR of everything before it
  EXPECT_EQ(bytes(encoder.get_battery_level_request()),
            (std::vector<uint8_t>{0x00, 0xff, 0x00, 0x00, 0x9a, 0xa2, 0x01, 0x01, 0x38}));
  EXPECT_EQ(bytes(encoder.get_light_level_request()),
            (std::vector<uint8_t>{0x00, 0xff, 0x00, 0x00, 0x9a, 0xaa, 0x01, 0x01, 0x30}));
  EXPECT_EQ(bytes(encoder.get_position_request()),
            (std::vector<uint8_t>{0x00, 0xff, 0x00, 0x00, 0x9a, 0xa7, 0x01, 0x01, 0x3d}));
  EXPECT_EQ(bytes(encoder.get_send_pin_request(0x1234)),
            (std::vector<uint8_t>{0x00, 0xff, 0x00, 0x00, 0x9a, 0x17, 0x02, 0x12, 0x34, 0xa9}));
  EXPECT_EQ(bytes(encoder.get_open_request()),
            (std::vector<uint8_t>{0x00, 0xff, 0x00, 0x00, 0x9a, 0x0a, 0x01, 0xdd, 0x4c}));
  EXPECT_EQ(bytes(encoder.get_close_request()),
            (std::vector<uint8_t>{0x00, 0xff, 0x00, 0x00, 0x9a, 0x0a, 0x01, 0xee, 0x7f}));
  EXPECT_EQ(bytes(encoder.get_stop_request()),
            (std::vector<uint8_t>{0x00, 0xff, 0x00, 0x00, 0x9a, 0x0a, 0x01, 0xcc, 0x5d}));
  EXPECT_EQ(bytes(encoder.get_set_position_request(42)),
            (std::vector<uint8_t>{0x00, 0xff, 0x00, 0x00, 0x9a, 0x0d, 0x01, 0x2a, 0xbc}));
}

TEST(Am43Decoder, ReadsEachResponse) {
  Am43Decoder decoder;
  const uint8_t battery[] = {0x9a, CMD_GET_BATTERY_LEVEL, 0, 0, 0, 0, 0, 77};
  decoder.decode(battery, sizeof(battery));
  ASSERT_TRUE(decoder.has_battery_level());
  EXPECT_EQ(decoder.battery_level_, 77);
  EXPECT_FALSE(decoder.has_position());

  const uint8_t light[] = {0x9a, CMD_GET_LIGHT_LEVEL, 0, 0, 9};
  decoder.decode(light, sizeof(light));
  ASSERT_TRUE(decoder.has_light_level());
  EXPECT_FLOAT_EQ(decoder.light_level_, 100.0f);
  EXPECT_FALSE(decoder.has_battery_level());  // flags reset on every frame

  const uint8_t position[] = {0x9a, CMD_GET_POSITION, 0, 0, 0, 30};
  decoder.decode(position, sizeof(position));
  ASSERT_TRUE(decoder.has_position());
  EXPECT_EQ(decoder.position_, 30);

  const uint8_t notify[] = {0x9a, CMD_NOTIFY_POSITION, 0, 0, 55};
  decoder.decode(notify, sizeof(notify));
  ASSERT_TRUE(decoder.has_position());
  EXPECT_EQ(decoder.position_, 55);

  const uint8_t pin_ack[] = {0x9a, CMD_SEND_PIN, 0, RESPONSE_ACK};
  decoder.decode(pin_ack, sizeof(pin_ack));
  ASSERT_TRUE(decoder.has_pin_response());
  EXPECT_TRUE(decoder.pin_ok_);

  const uint8_t position_nack[] = {0x9a, CMD_SET_POSITION, 0, RESPONSE_NACK};
  decoder.decode(position_nack, sizeof(position_nack));
  ASSERT_TRUE(decoder.has_set_position_response());
  EXPECT_FALSE(decoder.set_position_ok_);

  const uint8_t state_ack[] = {0x9a, CMD_SET_STATE, 0, RESPONSE_ACK};
  decoder.decode(state_ack, sizeof(state_ack));
  ASSERT_TRUE(decoder.has_set_state_response());
  EXPECT_TRUE(decoder.set_state_ok_);
}

TEST(Am43Decoder, IgnoresShortAndForeignFrames) {
  Am43Decoder decoder;
  const uint8_t short_battery[] = {0x9a, CMD_GET_BATTERY_LEVEL, 0, 0};
  decoder.decode(short_battery, sizeof(short_battery));
  EXPECT_FALSE(decoder.has_battery_level());

  const uint8_t foreign[] = {0x55, CMD_GET_POSITION, 0, 0, 0, 30};
  decoder.decode(foreign, sizeof(foreign));
  EXPECT_FALSE(decoder.has_position());
}

TEST(Am43Decoder, EachResponseNeedsItsMinimumLength) {
  struct Case {
    uint8_t command;
    uint8_t min_length;
    bool (*has)(Am43Decoder &);
  };
  const Case cases[] = {
      {CMD_GET_BATTERY_LEVEL, 8, [](Am43Decoder &d) { return d.has_battery_level(); }},
      {CMD_GET_LIGHT_LEVEL, 5, [](Am43Decoder &d) { return d.has_light_level(); }},
      {CMD_GET_POSITION, 6, [](Am43Decoder &d) { return d.has_position(); }},
      {CMD_NOTIFY_POSITION, 5, [](Am43Decoder &d) { return d.has_position(); }},
      {CMD_SEND_PIN, 4, [](Am43Decoder &d) { return d.has_pin_response(); }},
      {CMD_SET_POSITION, 4, [](Am43Decoder &d) { return d.has_set_position_response(); }},
      {CMD_SET_STATE, 4, [](Am43Decoder &d) { return d.has_set_state_response(); }},
  };
  for (const auto &c : cases) {
    uint8_t frame[8] = {0x9a, c.command, 0, RESPONSE_ACK, 0, 0, 0, 0};
    Am43Decoder decoder;
    decoder.decode(frame, c.min_length - 1);
    EXPECT_FALSE(c.has(decoder)) << "command 0x" << std::hex << int(c.command);
    decoder.decode(frame, c.min_length);
    EXPECT_TRUE(c.has(decoder)) << "command 0x" << std::hex << int(c.command);
  }
}

TEST(Am43Decoder, IgnoresTinyAndUnknownFrames) {
  auto expect_nothing = [](Am43Decoder &decoder) {
    EXPECT_FALSE(decoder.has_battery_level());
    EXPECT_FALSE(decoder.has_light_level());
    EXPECT_FALSE(decoder.has_position());
    EXPECT_FALSE(decoder.has_pin_response());
    EXPECT_FALSE(decoder.has_set_position_response());
    EXPECT_FALSE(decoder.has_set_state_response());
  };
  Am43Decoder decoder;
  const uint8_t one_byte[] = {0x9a};
  decoder.decode(one_byte, sizeof(one_byte));
  expect_nothing(decoder);
  const uint8_t unknown[] = {0x9a, 0x42, 0, RESPONSE_ACK, 0, 0, 0, 0};
  decoder.decode(unknown, sizeof(unknown));
  expect_nothing(decoder);
}

}  // namespace esphome::am43::testing
