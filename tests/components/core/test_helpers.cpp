#include <gtest/gtest.h>
#include <cstring>

#include "esphome/core/helpers.h"

namespace esphome::core::testing {

// --- format_hex_to() ---

TEST(FormatHexTo, Basic) {
  const uint8_t data[] = {0xAB, 0xCD, 0xEF};
  char buffer[7];  // 3 * 2 + 1
  format_hex_to(buffer, data, 3);
  EXPECT_STREQ(buffer, "abcdef");
}

TEST(FormatHexTo, SingleByte) {
  const uint8_t data[] = {0x0F};
  char buffer[3];
  format_hex_to(buffer, data, 1);
  EXPECT_STREQ(buffer, "0f");
}

TEST(FormatHexTo, ZeroLength) {
  char buffer[4] = "xxx";
  format_hex_to(buffer, static_cast<size_t>(sizeof(buffer)), static_cast<const uint8_t *>(nullptr), 0);
  EXPECT_STREQ(buffer, "");
}

TEST(FormatHexTo, ZeroBufferSize) {
  char buffer[4] = "xxx";
  const uint8_t data[] = {0xAB};
  format_hex_to(buffer, static_cast<size_t>(0), data, 1);
  // Should not crash, buffer unchanged
  EXPECT_EQ(buffer[0], 'x');
}

TEST(FormatHexTo, BufferTooSmall) {
  const uint8_t data[] = {0xAB, 0xCD, 0xEF};
  char buffer[5];  // only room for 2 bytes
  format_hex_to(buffer, data, 3);
  EXPECT_STREQ(buffer, "abcd");
}

TEST(FormatHexTo, MacAddress) {
  const uint8_t mac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
  char buffer[13];
  format_hex_to(buffer, mac, 6);
  EXPECT_STREQ(buffer, "aabbccddeeff");
}

// --- format_hex_pretty_to() ---

TEST(FormatHexPrettyTo, BasicColon) {
  const uint8_t data[] = {0xAB, 0xCD, 0xEF};
  char buffer[9];  // 3 * 3
  format_hex_pretty_to(buffer, data, 3);
  EXPECT_STREQ(buffer, "AB:CD:EF");
}

TEST(FormatHexPrettyTo, SingleByte) {
  const uint8_t data[] = {0x0F};
  char buffer[3];
  format_hex_pretty_to(buffer, data, 1);
  EXPECT_STREQ(buffer, "0F");
}

TEST(FormatHexPrettyTo, ZeroLength) {
  char buffer[4] = "xxx";
  format_hex_pretty_to(buffer, static_cast<size_t>(sizeof(buffer)), static_cast<const uint8_t *>(nullptr), 0);
  EXPECT_STREQ(buffer, "");
}

TEST(FormatHexPrettyTo, ZeroBufferSize) {
  char buffer[4] = "xxx";
  const uint8_t data[] = {0xAB};
  format_hex_pretty_to(buffer, static_cast<size_t>(0), data, 1);
  EXPECT_EQ(buffer[0], 'x');
}

TEST(FormatHexPrettyTo, CustomSeparator) {
  const uint8_t data[] = {0xAA, 0xBB, 0xCC};
  char buffer[9];
  format_hex_pretty_to(buffer, data, 3, '-');
  EXPECT_STREQ(buffer, "AA-BB-CC");
}

// --- format_mac_addr_upper() ---

TEST(FormatMacAddrUpper, Basic) {
  const uint8_t mac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
  char buffer[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  format_mac_addr_upper(mac, buffer);
  EXPECT_STREQ(buffer, "AA:BB:CC:DD:EE:FF");
}

TEST(FormatMacAddrUpper, AllZeros) {
  const uint8_t mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  char buffer[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  format_mac_addr_upper(mac, buffer);
  EXPECT_STREQ(buffer, "00:00:00:00:00:00");
}

// --- format_hex_char() ---

TEST(FormatHexChar, LowercaseDigits) {
  EXPECT_EQ(format_hex_char(0), '0');
  EXPECT_EQ(format_hex_char(9), '9');
  EXPECT_EQ(format_hex_char(10), 'a');
  EXPECT_EQ(format_hex_char(15), 'f');
}

TEST(FormatHexChar, UppercaseDigits) {
  EXPECT_EQ(format_hex_pretty_char(0), '0');
  EXPECT_EQ(format_hex_pretty_char(9), '9');
  EXPECT_EQ(format_hex_pretty_char(10), 'A');
  EXPECT_EQ(format_hex_pretty_char(15), 'F');
}

}  // namespace esphome::core::testing
