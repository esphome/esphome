#include <gtest/gtest.h>

#include "esphome/components/captive_portal/json_escape.h"

namespace esphome::captive_portal::testing {

// Plain ASCII with no special characters is passed through unchanged.
TEST(CaptivePortalJsonEscape, PlainStringUnchanged) {
  EXPECT_EQ(json_escape("MyNetwork"), "MyNetwork");
  EXPECT_EQ(json_escape(""), "");
}

// A double quote is escaped so it does not terminate the surrounding JSON string.
TEST(CaptivePortalJsonEscape, EscapesDoubleQuote) {
  EXPECT_EQ(json_escape("a\"b"), "a\\\"b");
  // A double quote followed by other characters stays inside the JSON string.
  EXPECT_EQ(json_escape("\">end"), "\\\">end");
}

// A backslash is doubled so it does not start an escape sequence in the output.
TEST(CaptivePortalJsonEscape, EscapesBackslash) {
  EXPECT_EQ(json_escape("a\\b"), "a\\\\b");
  // A trailing backslash must not escape the closing quote of the JSON string.
  EXPECT_EQ(json_escape("net\\"), "net\\\\");
}

// The control characters with short JSON forms use those forms.
TEST(CaptivePortalJsonEscape, EscapesShortFormControls) {
  EXPECT_EQ(json_escape("\n"), "\\n");
  EXPECT_EQ(json_escape("\r"), "\\r");
  EXPECT_EQ(json_escape("\t"), "\\t");
  EXPECT_EQ(json_escape("\b"), "\\b");
  EXPECT_EQ(json_escape("\f"), "\\f");
}

// Other control characters (< 0x20) without a short form become \u00XX with lowercase hex.
TEST(CaptivePortalJsonEscape, EscapesOtherControlsAsUnicode) {
  EXPECT_EQ(json_escape(std::string("\x00", 1)), "\\u0000");
  EXPECT_EQ(json_escape("\x01"), "\\u0001");
  EXPECT_EQ(json_escape("\x1f"), "\\u001f");
  // 0x7f (DEL) is >= 0x20, so it is NOT escaped by this helper.
  EXPECT_EQ(json_escape("\x7f"), "\x7f");
}

// Bytes >= 0x20, including multi-byte UTF-8 sequences, are passed through verbatim.
TEST(CaptivePortalJsonEscape, PassesThroughUtf8) {
  // "café" in UTF-8 (é == 0xC3 0xA9).
  EXPECT_EQ(json_escape("caf\xc3\xa9"), "caf\xc3\xa9");
  // Emoji (📶, 4-byte UTF-8) survives unchanged.
  EXPECT_EQ(json_escape("\xf0\x9f\x93\xb6"), "\xf0\x9f\x93\xb6");
}

// A mix of special and normal characters is escaped in place without disturbing the rest.
TEST(CaptivePortalJsonEscape, MixedContent) { EXPECT_EQ(json_escape("a\"b\\c\nd"), "a\\\"b\\\\c\\nd"); }

}  // namespace esphome::captive_portal::testing
