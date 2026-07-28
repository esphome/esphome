#include <gtest/gtest.h>

#include <string>

#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"

namespace esphome::testing {

namespace {

// Large enough that none of the inputs below are ever dropped.
constexpr size_t TEST_BUFFER_SIZE = 64 * JSON_ESCAPE_MAX_EXPANSION + 1;

// Escape into a stack buffer and return the result as a string so the expectations stay readable.
std::string escape(const std::string &value) {
  char buf[TEST_BUFFER_SIZE];
  return json_escape_into_buffer(buf, StringRef(value.c_str(), value.size()));
}

// Same, but with the short control forms turned off.
std::string escape_long(const std::string &value) {
  char buf[TEST_BUFFER_SIZE];
  return json_escape_into_buffer(buf, StringRef(value.c_str(), value.size()), false);
}

}  // namespace

// Plain ASCII with no special characters is passed through unchanged.
TEST(JsonEscape, PlainStringUnchanged) {
  EXPECT_EQ(escape("MyNetwork"), "MyNetwork");
  EXPECT_EQ(escape(""), "");
}

// A double quote is escaped so it does not terminate the surrounding JSON string.
TEST(JsonEscape, EscapesDoubleQuote) {
  EXPECT_EQ(escape("a\"b"), "a\\\"b");
  // A double quote followed by other characters stays inside the JSON string.
  EXPECT_EQ(escape("\">end"), "\\\">end");
}

// A backslash is doubled so it does not start an escape sequence in the output.
TEST(JsonEscape, EscapesBackslash) {
  EXPECT_EQ(escape("a\\b"), "a\\\\b");
  // A trailing backslash must not escape the closing quote of the JSON string.
  EXPECT_EQ(escape("net\\"), "net\\\\");
}

// The control characters with short JSON forms use those forms.
TEST(JsonEscape, EscapesShortFormControls) {
  EXPECT_EQ(escape("\n"), "\\n");
  EXPECT_EQ(escape("\r"), "\\r");
  EXPECT_EQ(escape("\t"), "\\t");
  EXPECT_EQ(escape("\b"), "\\b");
  EXPECT_EQ(escape("\f"), "\\f");
}

// Other control characters (< 0x20) without a short form become \u00XX with lowercase hex.
TEST(JsonEscape, EscapesOtherControlsAsUnicode) {
  EXPECT_EQ(escape(std::string("\x00", 1)), "\\u0000");
  EXPECT_EQ(escape("\x01"), "\\u0001");
  EXPECT_EQ(escape("\x10"), "\\u0010");
  EXPECT_EQ(escape("\x1f"), "\\u001f");
  // 0x7f (DEL) is >= 0x20, so it is NOT escaped by this helper.
  EXPECT_EQ(escape("\x7f"), "\x7f");
}

// With the short forms turned off, every control character is written as \u00XX instead.
TEST(JsonEscape, LongControlEscapes) {
  EXPECT_EQ(escape_long("\n"), "\\u000a");
  EXPECT_EQ(escape_long("\r"), "\\u000d");
  EXPECT_EQ(escape_long("\t"), "\\u0009");
  EXPECT_EQ(escape_long("\b"), "\\u0008");
  EXPECT_EQ(escape_long("\f"), "\\u000c");
  // Controls without a short form are unaffected by the flag.
  EXPECT_EQ(escape_long("\x01"), "\\u0001");
}

// The flag only affects control characters. A quote or backslash is never written as \u00XX, because that form is
// no shorter and both modes have always emitted the two character escape.
TEST(JsonEscape, LongModeStillUsesTwoCharQuoteAndBackslash) {
  EXPECT_EQ(escape_long("a\"b"), "a\\\"b");
  EXPECT_EQ(escape_long("a\\b"), "a\\\\b");
  // Ordinary text is untouched in either mode.
  EXPECT_EQ(escape_long("MyDevice"), "MyDevice");
}

// Bytes >= 0x20, including multi-byte UTF-8 sequences, are passed through verbatim.
TEST(JsonEscape, PassesThroughUtf8) {
  // "café" in UTF-8 (é == 0xC3 0xA9).
  EXPECT_EQ(escape("caf\xc3\xa9"), "caf\xc3\xa9");
  // Emoji (📶, 4-byte UTF-8) survives unchanged.
  EXPECT_EQ(escape("\xf0\x9f\x93\xb6"), "\xf0\x9f\x93\xb6");
}

// A mix of special and normal characters is escaped in place without disturbing the rest.
TEST(JsonEscape, MixedContent) { EXPECT_EQ(escape("a\"b\\c\nd"), "a\\\"b\\\\c\\nd"); }

// A buffer sized at JSON_ESCAPE_MAX_EXPANSION bytes per input byte holds the worst case exactly.
TEST(JsonEscape, WorstCaseInputFitsExactly) {
  constexpr size_t input_len = 8;
  char buf[input_len * JSON_ESCAPE_MAX_EXPANSION + 1];
  const std::string input(input_len, '\x01');
  std::string expected;
  for (size_t i = 0; i < input_len; i++)
    expected += "\\u0001";
  EXPECT_EQ(json_escape_into_buffer(buf, StringRef(input.c_str(), input.size())), expected);
}

// An escape sequence that would not fit is dropped whole rather than written partially, and the result stays null
// terminated.
TEST(JsonEscape, DropsEscapeThatWouldNotFit) {
  // Room for one \u00XX sequence plus the null terminator, but two are requested.
  char buf[JSON_ESCAPE_MAX_EXPANSION + 1];
  const std::string input(2, '\x01');
  const std::string result = json_escape_into_buffer(buf, StringRef(input.c_str(), input.size()));
  EXPECT_EQ(result, "\\u0001");
  EXPECT_EQ(buf[JSON_ESCAPE_MAX_EXPANSION], '\0');
}

// Plain characters are truncated at the buffer size, leaving room for the null terminator.
TEST(JsonEscape, TruncatesPlainInput) {
  char buf[5];
  const std::string input(20, 'a');
  EXPECT_STREQ(json_escape_into_buffer(buf, StringRef(input.c_str(), input.size())), "aaaa");
}

// A zero length buffer cannot even hold a null terminator, so an empty string is returned instead of writing.
TEST(JsonEscape, EmptyBufferIsSafe) {
  const std::string input("test");
  EXPECT_STREQ(json_escape_into_buffer(std::span<char>(), StringRef(input.c_str(), input.size())), "");
}

}  // namespace esphome::testing
