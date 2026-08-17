#include <gtest/gtest.h>
#include <cstring>

#include "esphome/core/alloc_helpers.h"
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

// --- small_pow10() ---

TEST(SmallPow10, Zero) { EXPECT_EQ(small_pow10(0), 1u); }
TEST(SmallPow10, One) { EXPECT_EQ(small_pow10(1), 10u); }
TEST(SmallPow10, Two) { EXPECT_EQ(small_pow10(2), 100u); }
TEST(SmallPow10, Three) { EXPECT_EQ(small_pow10(3), 1000u); }

// --- frac_to_str_unchecked() ---

TEST(FracToStr, OneDigit) {
  char buf[8];
  char *end = frac_to_str_unchecked(buf, 5, 1);
  *end = '\0';
  EXPECT_STREQ(buf, "5");
  EXPECT_EQ(end - buf, 1);
}

TEST(FracToStr, TwoDigits) {
  char buf[8];
  char *end = frac_to_str_unchecked(buf, 46, 10);
  *end = '\0';
  EXPECT_STREQ(buf, "46");
}

TEST(FracToStr, ThreeDigits) {
  char buf[8];
  char *end = frac_to_str_unchecked(buf, 456, 100);
  *end = '\0';
  EXPECT_STREQ(buf, "456");
  EXPECT_EQ(end - buf, 3);
}

TEST(FracToStr, LeadingZeros) {
  char buf[8];
  char *end = frac_to_str_unchecked(buf, 1, 100);
  *end = '\0';
  EXPECT_STREQ(buf, "001");

  end = frac_to_str_unchecked(buf, 5, 10);
  *end = '\0';
  EXPECT_STREQ(buf, "05");
}

TEST(FracToStr, AllZeros) {
  char buf[8];
  char *end = frac_to_str_unchecked(buf, 0, 100);
  *end = '\0';
  EXPECT_STREQ(buf, "000");

  end = frac_to_str_unchecked(buf, 0, 1);
  *end = '\0';
  EXPECT_STREQ(buf, "0");
}

TEST(FracToStr, ZeroDivisor) {
  char buf[8];
  buf[0] = 'X';
  char *end = frac_to_str_unchecked(buf, 0, 0);
  EXPECT_EQ(end, buf);  // writes nothing
}

// --- buf_append_sep_str() ---

TEST(BufAppendSepStr, Basic) {
  char buf[32] = "23.46";
  char *start = buf + 5;
  char *end = buf_append_sep_str(start, sizeof(buf) - 5, ' ', "°C", 3);
  EXPECT_STREQ(buf, "23.46 °C");
  EXPECT_EQ(end - buf, 9);  // "°C" is 3 bytes (UTF-8)
}

TEST(BufAppendSepStr, EmptyString) {
  char buf[32] = "100";
  char *start = buf + 3;
  char *end = buf_append_sep_str(start, sizeof(buf) - 3, ' ', "", 0);
  EXPECT_STREQ(buf, "100 ");
  EXPECT_EQ(end - start, 1);  // just the separator
}

TEST(BufAppendSepStr, NoRoom) {
  char buf[8] = "1234567";
  char *start = buf + 7;
  char *end = buf_append_sep_str(start, 1, ' ', "unit", 4);
  EXPECT_EQ(end, start);  // nothing written
}

TEST(BufAppendSepStr, Truncation) {
  char buf[8] = "val";
  char *start = buf + 3;
  // remaining = 5, separator takes 1, so 3 chars of string fit + null
  char *end = buf_append_sep_str(start, 5, ' ', "longunit", 8);
  *end = '\0';
  EXPECT_STREQ(buf, "val lon");
  EXPECT_EQ(end - buf, 7);
}

// --- base64 encode/decode ---

static const char BASE64_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Pack 6-bit indices 0..63 into 48 bytes so encoding yields the full alphabet in order
TEST(Base64, EncodeProducesCanonicalAlphabet) {
  uint8_t bytes[48];
  size_t n = 0;
  for (uint8_t i = 0; i < 64; i += 4) {
    bytes[n++] = (i << 2) | ((i + 1) >> 4);
    bytes[n++] = ((i + 1) & 0x0F) << 4 | ((i + 2) >> 2);
    bytes[n++] = ((i + 2) & 0x03) << 6 | (i + 3);
  }
  std::string encoded = base64_encode(bytes, sizeof(bytes));  // NOLINT(esphome-heap-allocation) - host test
  EXPECT_EQ(encoded, BASE64_ALPHABET);
}

// Decode the alphabet then re-encode: locks the encode and decode mappings together
TEST(Base64, DecodeCanonicalAlphabetRoundTrip) {
  uint8_t buf[48];
  size_t len = base64_decode(std::string(BASE64_ALPHABET), buf, sizeof(buf));
  EXPECT_EQ(len, 48u);
  std::string reencoded = base64_encode(buf, len);  // NOLINT(esphome-heap-allocation) - host test
  EXPECT_EQ(reencoded, BASE64_ALPHABET);
}

TEST(Base64, DecodeBase64UrlMatchesStandard) {
  std::string url = BASE64_ALPHABET;
  for (char &c : url) {
    if (c == '+')
      c = '-';
    if (c == '/')
      c = '_';
  }
  uint8_t standard[48], urlsafe[48];
  size_t len_standard = base64_decode(std::string(BASE64_ALPHABET), standard, sizeof(standard));
  size_t len_url = base64_decode(url, urlsafe, sizeof(urlsafe));
  EXPECT_EQ(len_standard, len_url);
  EXPECT_EQ(memcmp(standard, urlsafe, len_standard), 0);
}

// RFC 4648 vectors cover both padding cases (len % 3 == 1 and len % 3 == 2)
TEST(Base64, Rfc4648Vectors) {
  const struct {
    const char *plain;
    const char *encoded;
  } vectors[] = {
      {"", ""},
      {"f", "Zg=="},
      {"fo", "Zm8="},
      {"foo", "Zm9v"},
      {"foob", "Zm9vYg=="},
      {"fooba", "Zm9vYmE="},
      {"foobar", "Zm9vYmFy"},
  };
  for (const auto &v : vectors) {
    const auto *plain = reinterpret_cast<const uint8_t *>(v.plain);
    std::string encoded = base64_encode(plain, strlen(v.plain));  // NOLINT(esphome-heap-allocation) - host test
    EXPECT_EQ(encoded, v.encoded);
    uint8_t buf[8];
    size_t len = base64_decode(reinterpret_cast<const uint8_t *>(v.encoded), strlen(v.encoded), buf, sizeof(buf));
    EXPECT_EQ(len, strlen(v.plain));
    EXPECT_EQ(memcmp(buf, v.plain, len), 0);
  }
}

}  // namespace esphome::core::testing
