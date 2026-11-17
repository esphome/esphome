#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

}  // namespace esphome::core::testing
