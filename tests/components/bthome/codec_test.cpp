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

TEST(BTHomeCodec, ParsesPressBatteryAndMacIncluded) {
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

  const uint8_t hold_alias[] = {0x44, 0x00, 0x09, 0x3A, 0xFE};
  ASSERT_TRUE(parse(hold_alias, sizeof(hold_alias), &parsed));
  EXPECT_EQ(parsed.buttons[0], BUTTON_HOLD_ALIAS);

  uint8_t mac_included[1 + 6 + 4] = {0x46, 1, 2, 3, 4, 5, 6, 0x00, 0x03, 0x3A, 0x04};
  ASSERT_TRUE(parse(mac_included, sizeof(mac_included), &parsed));
  EXPECT_TRUE(parsed.mac_included);
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

TEST(BTHomeCodec, RejectsEncryptedAndVersion1) {
  const uint8_t encrypted[] = {0x45, 0x11, 0x22};
  Parsed parsed{};
  EXPECT_FALSE(parse(encrypted, sizeof(encrypted), &parsed));
  EXPECT_TRUE(parsed.encrypted);

  const uint8_t v1[] = {0x02, 0x3A, 0x01};
  EXPECT_FALSE(parse(v1, sizeof(v1), &parsed));
}

}  // namespace esphome::bthome::codec
