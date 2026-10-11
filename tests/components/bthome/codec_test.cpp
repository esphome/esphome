#include <gtest/gtest.h>

#include <cstdint>

#include "esphome/components/bthome/codec.h"

namespace esphome::bthome::testing {

using codec::parse;
using codec::Parsed;

TEST(BTHomeCodec, ParsesPressAndBattery) {
  const uint8_t press[] = {0x44, 0x00, 0x07, 0x3A, 0x01};
  Parsed parsed{};
  ASSERT_TRUE(parse(press, sizeof(press), &parsed));
  EXPECT_FALSE(parsed.encrypted);
  EXPECT_FALSE(parsed.has_mac);
  EXPECT_TRUE(parsed.has_packet_id);
  EXPECT_EQ(parsed.packet_id, 7);
  ASSERT_EQ(parsed.button_count, 1);
  EXPECT_EQ(parsed.buttons[0], 0x01);

  const uint8_t with_battery[] = {0x44, 0x00, 0x05, 0x01, 0x64, 0x3A, 0x01};
  ASSERT_TRUE(parse(with_battery, sizeof(with_battery), &parsed));
  EXPECT_EQ(parsed.packet_id, 5);
  ASSERT_EQ(parsed.button_count, 1);
  EXPECT_EQ(parsed.buttons[0], 0x01);

  // 0x64 and 0x65 are one byte each. The button after them stays.
  const uint8_t light_level[] = {0x44, 0x64, 0x02, 0x65, 0x03, 0x3A, 0x01};
  ASSERT_TRUE(parse(light_level, sizeof(light_level), &parsed));
  EXPECT_FALSE(parsed.has_packet_id);
  ASSERT_EQ(parsed.button_count, 1);
  EXPECT_EQ(parsed.buttons[0], 0x01);

  // 0xFE is kept raw. Mapping it to hold is the receiver's job.
  const uint8_t hold_alias[] = {0x44, 0x00, 0x09, 0x3A, 0xFE};
  ASSERT_TRUE(parse(hold_alias, sizeof(hold_alias), &parsed));
  EXPECT_EQ(parsed.buttons[0], 0xFE);
}

TEST(BTHomeCodec, ReadsTheMacThatBitOneAnnounces) {
  const uint8_t with_mac[] = {0x46, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x00, 0x03, 0x3A, 0x04};
  Parsed parsed{};
  ASSERT_TRUE(parse(with_mac, sizeof(with_mac), &parsed));
  ASSERT_TRUE(parsed.has_mac);
  const uint8_t mac[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  for (size_t i = 0; i < sizeof(mac); i++) {
    EXPECT_EQ(parsed.mac[i], mac[i]);
  }
  EXPECT_EQ(parsed.packet_id, 3);
  ASSERT_EQ(parsed.button_count, 1);
  EXPECT_EQ(parsed.buttons[0], 0x04);

  const uint8_t short_mac[] = {0x46, 0x11, 0x22, 0x33};
  EXPECT_FALSE(parse(short_mac, sizeof(short_mac), &parsed));

  // Bits 3 and 4 are reserved and add no bytes.
  const uint8_t reserved_bits[] = {0x5C, 0x00, 0x08, 0x3A, 0x02};
  ASSERT_TRUE(parse(reserved_bits, sizeof(reserved_bits), &parsed));
  EXPECT_FALSE(parsed.has_mac);
  EXPECT_EQ(parsed.packet_id, 8);
  EXPECT_EQ(parsed.buttons[0], 0x02);
}

TEST(BTHomeCodec, KeepsEarlierButtonsWhenALaterObjectStopsTheWalk) {
  const uint8_t unknown_after[] = {0x44, 0x3A, 0x01, 0x99};
  Parsed parsed{};
  ASSERT_TRUE(parse(unknown_after, sizeof(unknown_after), &parsed));
  ASSERT_EQ(parsed.button_count, 1);
  EXPECT_EQ(parsed.buttons[0], 0x01);

  // A zero-length text object is one length byte. The button after it stays.
  const uint8_t empty_text[] = {0x44, 0x53, 0x00, 0x3A, 0x01};
  ASSERT_TRUE(parse(empty_text, sizeof(empty_text), &parsed));
  ASSERT_EQ(parsed.button_count, 1);
  EXPECT_EQ(parsed.buttons[0], 0x01);

  const uint8_t raw_then_button[] = {0x44, 0x54, 0x02, 0xAA, 0xBB, 0x3A, 0x03};
  ASSERT_TRUE(parse(raw_then_button, sizeof(raw_then_button), &parsed));
  ASSERT_EQ(parsed.button_count, 1);
  EXPECT_EQ(parsed.buttons[0], 0x03);

  const uint8_t truncated_text[] = {0x44, 0x3A, 0x02, 0x53, 0x04, 0x41};
  ASSERT_TRUE(parse(truncated_text, sizeof(truncated_text), &parsed));
  ASSERT_EQ(parsed.button_count, 1);
  EXPECT_EQ(parsed.buttons[0], 0x02);

  const uint8_t missing_length[] = {0x44, 0x3A, 0x02, 0x54};
  ASSERT_TRUE(parse(missing_length, sizeof(missing_length), &parsed));
  ASSERT_EQ(parsed.button_count, 1);

  // Temperature needs two bytes, only one is left.
  const uint8_t truncated_fixed[] = {0x44, 0x3A, 0x01, 0x02, 0x10};
  ASSERT_TRUE(parse(truncated_fixed, sizeof(truncated_fixed), &parsed));
  ASSERT_EQ(parsed.button_count, 1);

  const uint8_t truncated_command[] = {0x44, 0x3A, 0x01, 0x3B, 0x02, 0x03};
  ASSERT_TRUE(parse(truncated_command, sizeof(truncated_command), &parsed));
  ASSERT_EQ(parsed.button_count, 1);

  const uint8_t command_without_header[] = {0x44, 0x3A, 0x01, 0x3B, 0x00};
  ASSERT_TRUE(parse(command_without_header, sizeof(command_without_header), &parsed));
  ASSERT_EQ(parsed.button_count, 1);
}

TEST(BTHomeCodec, SkipsDimmerAndCommandBeforeAButton) {
  // 0x3C is event plus steps. The button after it stays at index 1.
  const uint8_t dimmer[] = {0x44, 0x3C, 0x01, 0x03, 0x3A, 0x01};
  Parsed parsed{};
  ASSERT_TRUE(parse(dimmer, sizeof(dimmer), &parsed));
  ASSERT_EQ(parsed.button_count, 1);
  EXPECT_EQ(parsed.buttons[0], 0x01);

  // 0x3B: length byte (1 argument) plus the opcode. Then the button.
  const uint8_t command[] = {0x44, 0x3B, 0x01, 0x03, 0x05, 0x3A, 0x02};
  ASSERT_TRUE(parse(command, sizeof(command), &parsed));
  ASSERT_EQ(parsed.button_count, 1);
  EXPECT_EQ(parsed.buttons[0], 0x02);
}

TEST(BTHomeCodec, KeepsAtMostEightButtons) {
  const uint8_t nine[] = {0x44, 0x3A, 0x01, 0x3A, 0x02, 0x3A, 0x03, 0x3A, 0x04, 0x3A,
                          0x05, 0x3A, 0x06, 0x3A, 0x00, 0x3A, 0x80, 0x3A, 0x01};
  Parsed parsed{};
  ASSERT_TRUE(parse(nine, sizeof(nine), &parsed));
  ASSERT_EQ(parsed.button_count, codec::MAX_BUTTONS);
  EXPECT_EQ(parsed.buttons[codec::MAX_BUTTONS - 1], 0x80);
}

TEST(BTHomeCodec, RejectsEncryptedVersion1AndEmptyInput) {
  const uint8_t encrypted[] = {0x45, 0x11, 0x22};
  Parsed parsed{};
  EXPECT_FALSE(parse(encrypted, sizeof(encrypted), &parsed));
  EXPECT_TRUE(parsed.encrypted);

  const uint8_t v1[] = {0x02, 0x3A, 0x01};
  EXPECT_FALSE(parse(v1, sizeof(v1), &parsed));
  EXPECT_FALSE(parsed.encrypted);

  EXPECT_FALSE(parse(v1, 0, &parsed));
  EXPECT_FALSE(parse(nullptr, sizeof(v1), &parsed));
  EXPECT_FALSE(parse(v1, sizeof(v1), nullptr));
}

}  // namespace esphome::bthome::testing
