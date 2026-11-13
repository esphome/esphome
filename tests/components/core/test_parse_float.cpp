#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

#include "esphome/core/helpers.h"

namespace esphome::core::testing {

static uint32_t bits(float value) {
  uint32_t out;
  memcpy(&out, &value, sizeof(out));
  return out;
}

// strtof is the reference: every accepted input must give the same bits.
static void expect_matches_strtof(const char *str) {
  char *end = nullptr;
  const float expected = ::strtof(str, &end);
  const auto parsed = parse_number<float>(str);
  if (!parsed.has_value()) {
    ADD_FAILURE() << str << " was rejected";
    return;
  }
  const float actual = parsed.value();
  EXPECT_EQ(bits(actual), bits(expected)) << str << " parsed " << actual << " expected " << expected;
}

TEST(ParseFloat, PlainDecimalsMatchStrtof) {
  for (const char *str :
       {"0",     "-0",      "+0",        "1",        "-1",          "100.50",      "99.75",       "0.1", "0.3",
        "2.675", "9999999", "0.9999999", "1234.567", "-1234.567",   "0.000001",    "0.000000001", "12.", ".5",
        "-.5",   "007",     "00.50",     "123.4567", "0.009999999", "0.001234567", "-0.009999999"}) {
    expect_matches_strtof(str);
  }
}

TEST(ParseFloat, SweepOfTwoDecimalValuesMatchesStrtof) {
  char buf[16];
  for (int i = -20000; i <= 20000; i++) {
    snprintf(buf, sizeof(buf), "%s%d.%02d", i < 0 ? "-" : "", std::abs(i) / 100, std::abs(i) % 100);
    expect_matches_strtof(buf);
  }
}

// A fraction longer than the fast path accepts must fall back, however long it is.
TEST(ParseFloat, LongFractionsMatchStrtof) {
  std::string str = "0.";
  str.append(300, '0');
  str += "25";
  expect_matches_strtof(str.c_str());
  expect_matches_strtof(("-" + str).c_str());
  str = "1.";
  str.append(300, '0');
  expect_matches_strtof(str.c_str());
}

TEST(ParseFloat, LibcPathInputsMatchStrtof) {
  for (const char *str : {"1e3", "-2.5E-2", "0x10", "nan", " 12", "123456789", "16777215", "1.0000000", "0.0000000001",
                          "0.00000000001", "1.23456789", "3.4028235e38"}) {
    expect_matches_strtof(str);
  }
}

// Positive overflow and a positive literal infinity both come back as HUGE_VALF from strtof, and the parser
// rejects both; the negative forms pass through, as they did before the fast path.
TEST(ParseFloat, RejectsInvalidInput) {
  for (const char *str :
       {"", "-", "+", ".", "abc", "12abc", "1.2.3", "1,5", "12 ", "--1", "1e", "1e400", "inf", "infinity"}) {
    EXPECT_FALSE(parse_number<float>(str).has_value()) << str;
  }
}

// Above the midpoint 1 + 2^-24 by less than half a double: newlib's strtof rounds to the midpoint and then to
// 1.0f, as on the devices, where a strtof that rounds straight to a float gives the next float.
TEST(ParseFloat, FullPathRoundsLikeNewlib) {
  EXPECT_EQ(bits(parse_number<float>("1.0000000596046448").value()), 0x3F800000u);
  EXPECT_EQ(bits(parse_number<float>("-1.0000000596046448").value()), 0xBF800000u);
  // newlib stops after "nan" unless the parentheses hold only hex digits
  EXPECT_FALSE(parse_number<float>("nan(xyz)").has_value());
  EXPECT_TRUE(std::isnan(parse_number<float>("nan(12)").value()));
}

// newlib's strtof is (float) strtod with every NaN as nanf(""). The host's strtod rounds decimals to the nearest
// double just as newlib's does, so for decimal input this is newlib's strtof; hex and "nan(" input are compared
// with values taken from newlib itself below, as libcs differ there.
static float newlib_strtof(const char *str, char **end) {
  const double value = ::strtod(str, end);
  if (std::isnan(value))
    return std::signbit(value) ? -NAN : NAN;
  return static_cast<float>(value);
}

static void expect_prefix_matches_newlib(const char *str) {
  char *expected_end = nullptr;
  const float expected = newlib_strtof(str, &expected_end);
  char *end = nullptr;
  const float actual = parse_float_prefix(str, &end);
  EXPECT_EQ(bits(actual), bits(expected)) << '"' << str << '"';
  EXPECT_EQ(end - str, expected_end - str) << '"' << str << '"';
}

TEST(ParseFloatPrefix, RandomDecimalsMatchNewlib) {
  std::mt19937_64 rng(1);
  for (int i = 0; i < 200000; i++) {
    std::string str;
    if (rng() % 3 == 0)
      str += (rng() & 1) ? "-" : "+";
    const int digits = 1 + static_cast<int>(rng() % 25);
    const int point = static_cast<int>(rng() % (digits + 2)) - 1;
    for (int j = 0; j < digits; j++) {
      if (j == point)
        str += '.';
      str += static_cast<char>('0' + rng() % 10);
    }
    if (rng() & 1) {
      str += (rng() & 1) ? 'e' : 'E';
      char exp[8];
      snprintf(exp, sizeof(exp), "%d", static_cast<int>(rng() % 110) - 60);
      str += exp;
    }
    expect_prefix_matches_newlib(str.c_str());
  }
}

// Every 4099th float, printed with 6 to 17 digits, and the exact midpoint between it and the next float.
TEST(ParseFloatPrefix, FloatsAndMidpointsMatchNewlib) {
  char buf[160];
  for (uint32_t u = 0; u < 0x7F800000; u += 4099) {
    float f;
    memcpy(&f, &u, sizeof(f));
    const uint32_t next_bits = u + 1;
    float next;
    memcpy(&next, &next_bits, sizeof(next));
    for (int precision : {6, 9, 17}) {
      snprintf(buf, sizeof(buf), "%.*e", precision - 1, static_cast<double>(f));
      expect_prefix_matches_newlib(buf);
    }
    snprintf(buf, sizeof(buf), "%.120e", (static_cast<double>(f) + static_cast<double>(next)) / 2);
    expect_prefix_matches_newlib(buf);
  }
}

// Just above and just below a float midpoint, closer than half a double apart: strtod rounds to the midpoint and
// the float conversion then rounds it to the even neighbour, where rounding straight to a float would not.
TEST(ParseFloatPrefix, DoubleRoundingMatchesNewlib) {
  char buf[200];
  for (uint32_t u = 1; u < 0x7F800000; u += 65537) {
    float f;
    memcpy(&f, &u, sizeof(f));
    const uint32_t next_bits = u + 1;
    float next;
    memcpy(&next, &next_bits, sizeof(next));
    snprintf(buf, sizeof(buf), "%.120e", (static_cast<double>(f) + static_cast<double>(next)) / 2);
    std::string exact(buf);
    const size_t e = exact.find('e');
    size_t last = exact.find_last_not_of('0', e - 1);
    std::string above = exact.substr(0, last + 1) + "0000001" + exact.substr(e);
    expect_prefix_matches_newlib(above.c_str());
    std::string below = exact.substr(0, last + 1);
    below.back() = static_cast<char>(below.back() - 1);
    below += "9999999" + exact.substr(e);
    expect_prefix_matches_newlib(below.c_str());
  }
}

TEST(ParseFloatPrefix, SyntaxMatchesNewlib) {
  static const char *const TOKENS[] = {"0", "1", "9", ".",  "e", "E",   "+",   "-",     "i",  "n", "f", "a",
                                       "N", "I", " ", "\t", "y", "inf", "nan", "inity", "00", "#", ""};
  for (const char *a : TOKENS) {
    for (const char *b : TOKENS) {
      for (const char *c : TOKENS) {
        for (const char *d : TOKENS) {
          const std::string str = std::string(a) + b + c + d;
          expect_prefix_matches_newlib(str.c_str());
        }
      }
    }
  }
}

// Taken from newlib's strtof (newlib-xtensa d6e3fcdb, the ESP8266 toolchain's).
TEST(ParseFloatPrefix, MatchesNewlibWhereLibcsDiffer) {
  struct Case {
    const char *str;
    uint32_t bits;
    long end;
  };
  static const Case CASES[] = {
      // gethex rounds a tie up only when a bit below the second dropped bit is set
      {"0x1.100b0d0000000c", 0x3F880586, 18},
      {"0x1.0011010000000c", 0x3F800880, 18},
      {"0x1.0101010000000c0", 0x3F808080, 19},
      {"0x1.00000180000000001p0", 0x3F800001, 23},
      {"0x1.0000000000001p-150", 0x00000001, 22},
      {"0x1p-150", 0x00000000, 8},
      {"0x1.fffffefffffffp127", 0x7F7FFFFF, 21},
      {"0x1.ffffffp127", 0x7F800000, 14},
      {"0x.8", 0x3F000000, 4},
      {"0X1.8", 0x3FC00000, 5},
      {"0x1P-1", 0x3F000000, 6},
      {"0x1p+", 0x3F800000, 3},
      {"0x0p", 0x00000000, 3},
      {"0x.p1", 0x00000000, 1},
      {"-0x", 0x00000000, 2},  // no digit: only "0" is taken, without the sign
      // "nan(" is taken only up to a ")" with nothing but hex digits and characters up to ' ' before it
      {"nan()", 0x7FC00000, 5},
      {"nan( )", 0x7FC00000, 6},
      {"nan(12 ab)", 0x7FC00000, 10},
      {"nan(xyz)", 0x7FC00000, 3},
      {"nan(12", 0x7FC00000, 3},
      {"-nan(1)", 0xFFC00000, 7},
      // the exponent is capped at 19999
      {"1e999999999999", 0x7F800000, 14},
      {"1e-999999999999", 0x00000000, 15},
      {"0.000e99999", 0x00000000, 11},
  };
  for (const Case &c : CASES) {
    char *end = nullptr;
    const float value = parse_float_prefix(c.str, &end);
    EXPECT_EQ(bits(value), c.bits) << c.str;
    EXPECT_EQ(end - c.str, c.end) << c.str;
  }
}

TEST(ParseFloatPrefix, LongInputsMatchNewlib) {
  std::string str = "1.";
  str.append(3000, '0');
  str += "1e-3001";
  expect_prefix_matches_newlib(str.c_str());
  str = "0.";
  str.append(400, '0');
  str += "34028235";
  str += std::string(500, '7') + "e440";
  expect_prefix_matches_newlib(str.c_str());
  str = std::string(300, '9') + "e-261";
  expect_prefix_matches_newlib(str.c_str());
}

}  // namespace esphome::core::testing
