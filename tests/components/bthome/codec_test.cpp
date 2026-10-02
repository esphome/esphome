#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

#include "esphome/components/bthome/codec.h"

namespace esphome::bthome::codec {

TEST(BTHomeCodec, EncodesPressAndIndex) {
  uint8_t buf[32];
  size_t n = 0;
  ASSERT_TRUE(encode_button(7, 0x01, 1, buf, sizeof(buf), &n));
  const uint8_t press[] = {0x44, 0x00, 0x07, 0x3A, 0x01};
  ASSERT_EQ(n, sizeof(press));
  EXPECT_EQ(std::memcmp(buf, press, n), 0);

  ASSERT_TRUE(encode_button(1, 0x02, 2, buf, sizeof(buf), &n));
  const uint8_t second[] = {0x44, 0x00, 0x01, 0x3A, 0x00, 0x3A, 0x02};
  ASSERT_EQ(n, sizeof(second));
  EXPECT_EQ(std::memcmp(buf, second, n), 0);
  EXPECT_FALSE(encode_button(1, 0x01, 0, buf, sizeof(buf), &n));
}

TEST(BTHomeCodec, ParsesPressBatteryAndAReservedInfoBit) {
  const uint8_t press[] = {0x44, 0x00, 0x07, 0x3A, 0x01};
  Parsed parsed{};
  ASSERT_TRUE(parse(press, sizeof(press), &parsed));
  EXPECT_TRUE(parsed.ok);
  EXPECT_TRUE(parsed.trigger_based);
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
  ASSERT_EQ(parsed.button_count, 1);
  EXPECT_EQ(parsed.buttons[0], 0x01);

  // 0xFE is kept raw. Mapping it to hold is the receiver's job.
  const uint8_t hold_alias[] = {0x44, 0x00, 0x09, 0x3A, 0xFE};
  ASSERT_TRUE(parse(hold_alias, sizeof(hold_alias), &parsed));
  EXPECT_EQ(parsed.buttons[0], 0xFE);

  // Bit 1 is reserved. A button still starts at the next byte.
  const uint8_t reserved_bit[] = {0x46, 0x00, 0x03, 0x3A, 0x04};
  ASSERT_TRUE(parse(reserved_bit, sizeof(reserved_bit), &parsed));
  EXPECT_FALSE(parsed.encrypted);
  EXPECT_TRUE(parsed.trigger_based);
  EXPECT_EQ(parsed.packet_id, 3);
  EXPECT_EQ(parsed.buttons[0], 0x04);
}

TEST(BTHomeCodec, KeepsEarlierButtonsWhenALaterObjectStopsTheWalk) {
  const uint8_t unknown_after[] = {0x44, 0x3A, 0x01, 0x99};
  Parsed parsed{};
  ASSERT_TRUE(parse(unknown_after, sizeof(unknown_after), &parsed));
  EXPECT_TRUE(parsed.ok);
  ASSERT_EQ(parsed.button_count, 1);
  EXPECT_EQ(parsed.buttons[0], 0x01);

  // A zero-length text object is one length byte. The button after it stays.
  const uint8_t empty_text[] = {0x44, 0x53, 0x00, 0x3A, 0x01};
  ASSERT_TRUE(parse(empty_text, sizeof(empty_text), &parsed));
  ASSERT_EQ(parsed.button_count, 1);
  EXPECT_EQ(parsed.buttons[0], 0x01);

  const uint8_t truncated_text[] = {0x44, 0x3A, 0x02, 0x53, 0x04, 0x41};
  ASSERT_TRUE(parse(truncated_text, sizeof(truncated_text), &parsed));
  ASSERT_EQ(parsed.button_count, 1);
  EXPECT_EQ(parsed.buttons[0], 0x02);
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

TEST(BTHomeCodec, RejectsEncryptedAndVersion1) {
  const uint8_t encrypted[] = {0x45, 0x11, 0x22};
  Parsed parsed{};
  EXPECT_FALSE(parse(encrypted, sizeof(encrypted), &parsed));
  EXPECT_TRUE(parsed.encrypted);

  const uint8_t v1[] = {0x02, 0x3A, 0x01};
  EXPECT_FALSE(parse(v1, sizeof(v1), &parsed));
}

}  // namespace esphome::bthome::codec
