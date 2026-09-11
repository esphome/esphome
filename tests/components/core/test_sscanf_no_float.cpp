#include <gtest/gtest.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "esphome/core/sscanf_no_float.h"

namespace esphome::core::testing {

// --- The two bluedroid call sites the wrap exists for ---

TEST(SscanfNoFloat, BluedroidHexByte) {
  unsigned int value = 0;
  EXPECT_EQ(sscanf_no_float("ab", "%02x", &value), 1);
  EXPECT_EQ(value, 0xabu);
  // Width stops after two digits even when more follow
  EXPECT_EQ(sscanf_no_float("ab12", "%02x", &value), 1);
  EXPECT_EQ(value, 0xabu);
}

TEST(SscanfNoFloat, BluedroidAddress) {
  uint32_t a[6] = {0};
  EXPECT_EQ(sscanf_no_float("AA:bb:0C:dd:ee:FF", "%02x:%02x:%02x:%02x:%02x:%02x", a, a + 1, a + 2, a + 3, a + 4, a + 5),
            6);
  EXPECT_EQ(a[0], 0xaau);
  EXPECT_EQ(a[1], 0xbbu);
  EXPECT_EQ(a[2], 0x0cu);
  EXPECT_EQ(a[3], 0xddu);
  EXPECT_EQ(a[4], 0xeeu);
  EXPECT_EQ(a[5], 0xffu);
  // Short input assigns what matched, like libc
  EXPECT_EQ(sscanf_no_float("AA:bb:0C", "%02x:%02x:%02x:%02x:%02x:%02x", a, a + 1, a + 2, a + 3, a + 4, a + 5), 3);
}

// --- The formats newlib's tzset uses, in case a wrap ever covers siscanf ---

TEST(SscanfNoFloat, TzsetName) {
  char name[12] = {0};
  int consumed = 0;
  EXPECT_EQ(sscanf_no_float("CST6CDT,M3.2.0,M11.1.0", "%11[-+0-9A-Za-z]%n", name, &consumed), 1);
  EXPECT_STREQ(name, "CST6CDT");
  EXPECT_EQ(consumed, 7);
}

TEST(SscanfNoFloat, TzsetOffsetAndRule) {
  uint16_t h = 0, m = 0, s = 0;
  int n1 = 0, n2 = 0, n3 = 0;
  EXPECT_EQ(sscanf_no_float("6:30:15", "%hu%n:%hu%n:%hu%n", &h, &n1, &m, &n2, &s, &n3), 3);
  EXPECT_EQ(h, 6);
  EXPECT_EQ(m, 30);
  EXPECT_EQ(s, 15);
  EXPECT_EQ(n1, 1);
  EXPECT_EQ(n2, 4);
  EXPECT_EQ(n3, 7);
  EXPECT_EQ(sscanf_no_float("M3.2.0", "M%hu%n.%hu%n.%hu%n", &h, &n1, &m, &n2, &s, &n3), 3);
  EXPECT_EQ(sscanf_no_float("X3.2.0", "M%hu", &h), 0);
}

// --- Integer conversions ---

TEST(SscanfNoFloat, SignedDecimalSkipsLeadingSpace) {
  int value = 0;
  EXPECT_EQ(sscanf_no_float("  -42 rest", "%d", &value), 1);
  EXPECT_EQ(value, -42);
  EXPECT_EQ(sscanf_no_float("+7", "%u", &value), 1);
  EXPECT_EQ(value, 7);
}

TEST(SscanfNoFloat, AutoBase) {
  int value = 0;
  EXPECT_EQ(sscanf_no_float("0x1F", "%i", &value), 1);
  EXPECT_EQ(value, 31);
  EXPECT_EQ(sscanf_no_float("017", "%i", &value), 1);
  EXPECT_EQ(value, 15);
  EXPECT_EQ(sscanf_no_float("0", "%i", &value), 1);
  EXPECT_EQ(value, 0);
  EXPECT_EQ(sscanf_no_float("-0x10", "%i", &value), 1);
  EXPECT_EQ(value, -16);
}

TEST(SscanfNoFloat, OctalAndHexPrefix) {
  unsigned int value = 0;
  EXPECT_EQ(sscanf_no_float("17", "%o", &value), 1);
  EXPECT_EQ(value, 15u);
  EXPECT_EQ(sscanf_no_float("+0x10", "%x", &value), 1);
  EXPECT_EQ(value, 16u);
  // "0x" with no digit after it: only the 0 is consumed
  EXPECT_EQ(sscanf_no_float("0xg", "%x", &value), 1);
  EXPECT_EQ(value, 0u);
  EXPECT_EQ(sscanf_no_float("zz", "%x", &value), 0);
}

TEST(SscanfNoFloat, WidthSplitsDigits) {
  int a = 0;
  unsigned int b = 0;
  EXPECT_EQ(sscanf_no_float("12345", "%3d%d", &a, &b), 2);
  EXPECT_EQ(a, 123);
  EXPECT_EQ(b, 45u);
  EXPECT_EQ(sscanf_no_float("ff", "%1x", &b), 1);
  EXPECT_EQ(b, 15u);
}

TEST(SscanfNoFloat, LengthModifiers) {
  unsigned char hh = 0;
  int16_t h = 0;
  long l = 0;  // NOLINT(google-runtime-int) %ld is defined in terms of long
  uint64_t ll = 0;
  size_t z = 0;
  ptrdiff_t t = 0;
  EXPECT_EQ(sscanf_no_float("255", "%hhu", &hh), 1);
  EXPECT_EQ(hh, 255);
  EXPECT_EQ(sscanf_no_float("-2", "%hd", &h), 1);
  EXPECT_EQ(h, -2);
  EXPECT_EQ(sscanf_no_float("-1", "%ld", &l), 1);
  EXPECT_EQ(l, -1L);
  EXPECT_EQ(sscanf_no_float("18446744073709551615", "%llu", &ll), 1);
  EXPECT_EQ(ll, UINT64_MAX);
  EXPECT_EQ(sscanf_no_float("123", "%ju", &ll), 1);
  EXPECT_EQ(ll, 123u);
  EXPECT_EQ(sscanf_no_float("9", "%zu", &z), 1);
  EXPECT_EQ(z, 9u);
  EXPECT_EQ(sscanf_no_float("-9", "%td", &t), 1);
  EXPECT_EQ(t, -9);
}

TEST(SscanfNoFloat, Suppression) {
  int value = 0;
  EXPECT_EQ(sscanf_no_float("1 2 3", "%*d %d %*d", &value), 1);
  EXPECT_EQ(value, 2);
}

// --- Character, string and scanset conversions ---

TEST(SscanfNoFloat, CharDoesNotSkipSpace) {
  char c[3] = {0};
  EXPECT_EQ(sscanf_no_float("xyz", "%c%c", c, c + 1), 2);
  EXPECT_EQ(c[0], 'x');
  EXPECT_EQ(c[1], 'y');
  EXPECT_EQ(sscanf_no_float("  x", "%c", c), 1);
  EXPECT_EQ(c[0], ' ');
}

TEST(SscanfNoFloat, CharWidthShortRead) {
  char c[4] = {0};
  EXPECT_EQ(sscanf_no_float("abc", "%2c", c), 1);
  EXPECT_EQ(c[0], 'a');
  EXPECT_EQ(c[1], 'b');
  EXPECT_EQ(c[2], '\0');
  // Fewer characters than the width still count as one assignment, like newlib
  EXPECT_EQ(sscanf_no_float("ab", "%3c", c), 1);
  EXPECT_EQ(sscanf_no_float("", "%c", c), EOF);
}

TEST(SscanfNoFloat, StringStopsAtSpaceAndWidth) {
  char buf[16] = {0};
  EXPECT_EQ(sscanf_no_float("hello world", "%5s", buf), 1);
  EXPECT_STREQ(buf, "hello");
  EXPECT_EQ(sscanf_no_float("  x", "%1s", buf), 1);
  EXPECT_STREQ(buf, "x");
  EXPECT_EQ(sscanf_no_float("   ", "%s", buf), EOF);
}

TEST(SscanfNoFloat, Scansets) {
  char buf[16] = {0};
  char rest[16] = {0};
  EXPECT_EQ(sscanf_no_float("key=val", "%[^=]=%s", buf, rest), 2);
  EXPECT_STREQ(buf, "key");
  EXPECT_STREQ(rest, "val");
  // ']' first is a member; '-' last is literal
  EXPECT_EQ(sscanf_no_float("]]ab", "%[]]", buf), 1);
  EXPECT_STREQ(buf, "]]");
  EXPECT_EQ(sscanf_no_float("a-z]x", "%[a-]", buf), 1);
  EXPECT_STREQ(buf, "a-");
  EXPECT_EQ(sscanf_no_float("abc", "%[^]]", buf), 1);
  EXPECT_STREQ(buf, "abc");
  // No member matched is a matching failure
  EXPECT_EQ(sscanf_no_float("123", "%[a-z]", buf), 0);
}

// --- Literals, whitespace and return value ---

TEST(SscanfNoFloat, LiteralsAndPercent) {
  int value = 0;
  EXPECT_EQ(sscanf_no_float("50%", "%d%%", &value), 1);
  EXPECT_EQ(value, 50);
  EXPECT_EQ(sscanf_no_float("50 %", "%d%%", &value), 1);
  EXPECT_EQ(sscanf_no_float("1,2", "%d,%d", &value, &value), 2);
  EXPECT_EQ(sscanf_no_float("1 ,2", "%d,%d", &value, &value), 1);
  EXPECT_EQ(sscanf_no_float("1, 2", "%d,%d", &value, &value), 2);
  EXPECT_EQ(sscanf_no_float("12\n34", "%d%d", &value, &value), 2);
}

TEST(SscanfNoFloat, PositionOnly) {
  int n = 0;
  EXPECT_EQ(sscanf_no_float("ab", "ab%n", &n), 0);
  EXPECT_EQ(n, 2);
  EXPECT_EQ(sscanf_no_float("", "%n", &n), 0);
  EXPECT_EQ(n, 0);
}

TEST(SscanfNoFloat, EofVersusMatchingFailure) {
  int value = 0;
  EXPECT_EQ(sscanf_no_float("", "%d", &value), EOF);
  EXPECT_EQ(sscanf_no_float("   ", "%d", &value), EOF);
  EXPECT_EQ(sscanf_no_float("a", "ab"), EOF);
  EXPECT_EQ(sscanf_no_float("abc", "%d", &value), 0);
  EXPECT_EQ(sscanf_no_float("ac", "ab"), 0);
  EXPECT_EQ(sscanf_no_float("12abc", "%d%d", &value, &value), 1);
  EXPECT_EQ(sscanf_no_float("12", "%d%d", &value, &value), 1);
}

// --- Conversions the scanner refuses rather than misparses ---

TEST(SscanfNoFloat, UnsupportedConversions) {
  float f = 0;
  double d = 0;
  void *p = nullptr;
  wchar_t w[4] = {0};
  EXPECT_EQ(sscanf_no_float("1.5", "%f", &f), SSCANF_UNSUPPORTED);
  EXPECT_EQ(sscanf_no_float("1.5", "%lf", &d), SSCANF_UNSUPPORTED);
  EXPECT_EQ(sscanf_no_float("1e3", "%g", &d), SSCANF_UNSUPPORTED);
  EXPECT_EQ(sscanf_no_float("0x1p3", "%a", &d), SSCANF_UNSUPPORTED);
  EXPECT_EQ(sscanf_no_float("0x10", "%p", &p), SSCANF_UNSUPPORTED);
  EXPECT_EQ(sscanf_no_float("abc", "%ls", w), SSCANF_UNSUPPORTED);
  EXPECT_EQ(sscanf_no_float("abc", "%lc", w), SSCANF_UNSUPPORTED);
  // Earlier conversions do not hide a later unsupported one
  int value = 0;
  EXPECT_EQ(sscanf_no_float("1 2.5", "%d %f", &value, &f), SSCANF_UNSUPPORTED);
}

}  // namespace esphome::core::testing
