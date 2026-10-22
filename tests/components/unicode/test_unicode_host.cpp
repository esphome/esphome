#include "esphome/components/unicode/unicode.h"

#include <gtest/gtest.h>

#include <array>
#include <cstring>

namespace esphome::unicode::testing {

struct Decoded {
  uint32_t code_point;
  size_t length;
};

// Decodes the first sequence of text placed right before the end of a small buffer, so a read past the
// terminator lands on the guard bytes that make the result wrong instead of silently passing
static Decoded decode(const char *text) {
  std::array<char, 8> buffer{};
  const size_t len = std::strlen(text);
  std::memcpy(buffer.data(), text, len);
  buffer[len] = '\0';
  for (size_t i = len + 1; i < buffer.size(); i++) {
    buffer[i] = static_cast<char>(0x80);  // looks like a continuation byte to a decoder that reads on
  }
  Decoded result{};
  result.code_point = extract_unicode_codepoint(buffer.data(), &result.length);
  return result;
}

TEST(Unicode, DecodesEachSequenceLength) {
  EXPECT_EQ(decode("A").code_point, 0x41u);
  EXPECT_EQ(decode("A").length, 1u);
  EXPECT_EQ(decode("\xC3\xA9").code_point, 0xE9u);  // é
  EXPECT_EQ(decode("\xC3\xA9").length, 2u);
  EXPECT_EQ(decode("\xE2\x82\xAC").code_point, 0x20ACu);  // €
  EXPECT_EQ(decode("\xE2\x82\xAC").length, 3u);
  EXPECT_EQ(decode("\xF0\x9F\x98\x80").code_point, 0x1F600u);  // 😀
  EXPECT_EQ(decode("\xF0\x9F\x98\x80").length, 4u);
}

TEST(Unicode, EmptyStringGivesNothing) {
  EXPECT_EQ(decode("").code_point, 0u);
  EXPECT_EQ(decode("").length, 0u);
}

TEST(Unicode, TruncatedSequenceStopsAtTheTerminator) {
  for (const char *text : {"\xC3", "\xE2\x82", "\xE2", "\xF0\x9F\x98", "\xF0\x9F", "\xF0"}) {
    EXPECT_EQ(decode(text).code_point, 0u) << text;
    EXPECT_EQ(decode(text).length, 0u) << text;
  }
}

TEST(Unicode, RejectsMalformedSequences) {
  EXPECT_EQ(decode("\x80").length, 0u);                  // lone continuation byte
  EXPECT_EQ(decode("\xF8\x88\x80\x80\x80").length, 0u);  // 5-byte lead
  EXPECT_EQ(decode("\xC0\xAF").length, 0u);              // overlong '/'
  EXPECT_EQ(decode("\xE0\x80\xAF").length, 0u);          // overlong '/'
  EXPECT_EQ(decode("\xF0\x80\x80\xAF").length, 0u);      // overlong '/'
  EXPECT_EQ(decode("\xED\xA0\x80").length, 0u);          // surrogate U+D800
  EXPECT_EQ(decode("\xF4\x90\x80\x80").length, 0u);      // U+110000, past the last code point
  EXPECT_EQ(decode("\xC3\x41").length, 0u);              // second byte is not a continuation byte
}

}  // namespace esphome::unicode::testing
