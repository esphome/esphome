#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <cstring>

#include "esphome/components/esp32_ble/bluedroid_hex.h"

namespace esphome::esp32_ble::testing {

namespace {
constexpr std::array<uint8_t, 6> UNTOUCHED = {0xde, 0xad, 0xbe, 0xef, 0x55, 0xaa};

std::array<uint8_t, 6> bdaddr_of(const char *str, bool expect_ok = true) {
  std::array<uint8_t, 6> addr = UNTOUCHED;
  EXPECT_EQ(parse_bdaddr(str, addr.data()), expect_ok) << str;
  return addr;
}

// btc_config_get_str: strlcpy the stored string, then report strlen + 1
bool config_get_bin_of(const char *stored, uint8_t *value, size_t *length) {
  auto get_str = [stored](const char *, const char *, char *out, int *size_bytes) {
    if (stored == nullptr)
      return false;
    size_t n = std::min(strlen(stored), static_cast<size_t>(*size_bytes) - 1);
    memcpy(out, stored, n);
    out[n] = '\0';
    *size_bytes = static_cast<int>(n) + 1;
    return true;
  };
  return config_get_bin(get_str, "section", "key", value, length);
}
}  // namespace

TEST(BluedroidHex, BdaddrTwoDigits) {
  EXPECT_EQ(bdaddr_of("aa:BB:0c:d1:Ee:ff"), (std::array<uint8_t, 6>{0xaa, 0xbb, 0x0c, 0xd1, 0xee, 0xff}));
}

TEST(BluedroidHex, BdaddrOneDigit) {
  EXPECT_EQ(bdaddr_of("1:2:a:B:0:f"), (std::array<uint8_t, 6>{0x01, 0x02, 0x0a, 0x0b, 0x00, 0x0f}));
  EXPECT_EQ(bdaddr_of("12:3:45:6:78:9"), (std::array<uint8_t, 6>{0x12, 0x03, 0x45, 0x06, 0x78, 0x09}));
}

TEST(BluedroidHex, BdaddrIgnoresTrailingText) {
  EXPECT_EQ(bdaddr_of("01:02:03:04:05:06:07"), (std::array<uint8_t, 6>{1, 2, 3, 4, 5, 6}));
  // %02x stops after two digits, so a third digit in the last byte is ignored too
  EXPECT_EQ(bdaddr_of("01:02:03:04:05:067"), (std::array<uint8_t, 6>{1, 2, 3, 4, 5, 0x06}));
  EXPECT_EQ(bdaddr_of("01:02:03:04:05:06 extra"), (std::array<uint8_t, 6>{1, 2, 3, 4, 5, 6}));
}

TEST(BluedroidHex, BdaddrRejectsAndLeavesOutputUntouched) {
  for (const char *bad :
       {"", ":01:02:03:04:05", "01:02:03:04:05", "01:02:03:04:05:", "01:02:03:04:05:g6", "01-02-03-04-05-06",
        "012:02:03:04:05:06", "01:02::04:05:06", "0x:02:03:04:05:06", " 1:02:03:04:05:06"}) {
    EXPECT_EQ(bdaddr_of(bad, false), UNTOUCHED) << bad;
  }
}

TEST(BluedroidHex, ConfigBinDecodes) {
  std::array<uint8_t, 6> value{};
  size_t length = value.size();
  ASSERT_TRUE(parse_config_bin("00ff7Ea5", 8, value.data(), &length));
  EXPECT_EQ(length, 4u);
  EXPECT_EQ(value, (std::array<uint8_t, 6>{0x00, 0xff, 0x7e, 0xa5, 0, 0}));
}

TEST(BluedroidHex, ConfigBinExactCapacityAndEmpty) {
  std::array<uint8_t, 2> value{};
  size_t length = value.size();
  ASSERT_TRUE(parse_config_bin("1234", 4, value.data(), &length));
  EXPECT_EQ(length, 2u);
  EXPECT_EQ(value, (std::array<uint8_t, 2>{0x12, 0x34}));

  length = value.size();
  ASSERT_TRUE(parse_config_bin("", 0, value.data(), &length));
  EXPECT_EQ(length, 0u);
}

TEST(BluedroidHex, ConfigBinRejectsAndLeavesOutputUntouched) {
  struct Case {
    const char *str;
    size_t capacity;
  };
  // odd length, more bytes than the capacity, a non-hex digit
  for (const Case &c : {Case{"123", 4}, Case{"123456", 2}, Case{"12g4", 4}, Case{"12 4", 4}, Case{"0x12", 4}}) {
    uint8_t value[4] = {0xaa, 0xaa, 0xaa, 0xaa};
    size_t length = c.capacity;
    EXPECT_FALSE(parse_config_bin(c.str, strlen(c.str), value, &length)) << c.str;
    EXPECT_EQ(length, c.capacity) << c.str;
    for (uint8_t b : value)
      EXPECT_EQ(b, 0xaa) << c.str;
  }
}

TEST(BluedroidHex, ConfigGetBinThroughStore) {
  uint8_t value[CONFIG_BIN_STACK_BYTES + 8] = {};
  size_t length = 3;
  ASSERT_TRUE(config_get_bin_of("0a0b0c", value, &length));
  EXPECT_EQ(length, 3u);
  EXPECT_EQ(value[2], 0x0c);

  length = 3;
  EXPECT_FALSE(config_get_bin_of(nullptr, value, &length));
  EXPECT_EQ(length, 3u);
}

TEST(BluedroidHex, ConfigGetBinRejectsTooLongAfterTruncation) {
  uint8_t value[4] = {0xaa, 0xaa, 0xaa, 0xaa};
  // One and several bytes more than the capacity: both truncate to an odd length
  for (const char *stored : {"0102030405", "010203040506070809"}) {
    size_t length = 4;
    EXPECT_FALSE(config_get_bin_of(stored, value, &length)) << stored;
    EXPECT_EQ(length, 4u);
    EXPECT_EQ(value[0], 0xaa);
  }
}

TEST(BluedroidHex, ConfigGetBinHeapFallback) {
  // Larger than the stack buffer, like a BT Classic HID descriptor
  constexpr size_t big = CONFIG_BIN_STACK_BYTES * 4;
  char stored[2 * big + 1];
  for (size_t i = 0; i < big; i++)
    snprintf(stored + 2 * i, 3, "%02x", static_cast<unsigned>(i));
  uint8_t value[big];
  size_t length = big;
  ASSERT_TRUE(config_get_bin_of(stored, value, &length));
  EXPECT_EQ(length, big);
  for (size_t i = 0; i < big; i++)
    EXPECT_EQ(value[i], static_cast<uint8_t>(i));
}

}  // namespace esphome::esp32_ble::testing
