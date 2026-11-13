#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

#include "esphome/core/helpers.h"
#include "esphome/core/parse_float.h"

#ifdef USE_HOST
namespace esphome::core::testing {

static uint32_t bits(float value) { return bit_cast<uint32_t>(value); }

// The host's strtof (glibc, macOS) rounds decimals straight to the nearest float, ties to even, as the parser
// does, with every NaN as nanf(""). For decimal input its end matches newlib's; hex and "nan(" input are compared
// with ends taken from newlib itself, as libcs differ there.
static float reference_strtof(const char *str, char **end) {
  const float value = ::strtof(str, end);
  if (std::isnan(value))
    return std::signbit(value) ? -NAN : NAN;
  return value;
}

// Every accepted input must give the reference's bits.
static void expect_matches_strtof(const char *str) {
  const float expected = reference_strtof(str, nullptr);
  const auto parsed = parse_number<float>(str);
  if (!parsed.has_value()) {
    ADD_FAILURE() << str << " was rejected";
    return;
  }
  EXPECT_EQ(bits(parsed.value()), bits(expected)) << str;
}

// The same bits and end as the reference, within max_steps exact comparison rounds: the guess is within one float,
// so at most two (the parser caps them at 3)
static void expect_str_to_float_matches(const char *str, uint32_t max_steps = 2) {
  char *expected_end = nullptr;
  const float expected = reference_strtof(str, &expected_end);
  char *end = nullptr;
  const uint32_t steps = internal::parse_float_exact_steps();
  const float actual = str_to_float(str, &end);
  EXPECT_LE(internal::parse_float_exact_steps() - steps, max_steps) << '"' << str << '"';
  EXPECT_EQ(bits(actual), bits(expected)) << '"' << str << '"';
  EXPECT_EQ(end - str, expected_end - str) << '"' << str << '"';
}

static float float_from(uint32_t value) { return bit_cast<float>(value); }

// The exact decimal of the midpoint between the floats with bits k and k + 1
static std::string midpoint_string(uint32_t k) {
  char buf[200];
  snprintf(buf, sizeof(buf), "%.120e", (static_cast<double>(float_from(k)) + float_from(k + 1)) / 2);
  return buf;
}

// That midpoint with digits appended after its last non-zero digit (just above it), or with the last digit one
// lower and digits appended (just below it)
static std::string midpoint_with_suffix(uint32_t k, const std::string &suffix, bool below = false) {
  const std::string exact = midpoint_string(k);
  const size_t e = exact.find('e');
  std::string mantissa = exact.substr(0, exact.find_last_not_of('0', e - 1) + 1);
  if (below)
    mantissa.back() = static_cast<char>(mantissa.back() - 1);
  return mantissa + suffix + exact.substr(e);
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
// 1.0f; the correctly rounded float, picolibc's, is the next one.
TEST(ParseFloat, FullPathRoundsCorrectly) {
  EXPECT_EQ(bits(parse_number<float>("1.0000000596046448").value()), 0x3F800001u);
  EXPECT_EQ(bits(parse_number<float>("-1.0000000596046448").value()), 0xBF800001u);
}

// newlib stops after "nan" unless the parentheses hold only hex digits, so parse_number<float> rejects the rest
TEST(ParseFloat, NanForms) {
  EXPECT_TRUE(std::isnan(parse_number<float>("nan(12)").value()));
  EXPECT_FALSE(parse_number<float>("nan(xyz)").has_value());
}

// The callers' overflow check: a positive overflow is HUGE_VALF and rejected, a negative one is -inf and accepted
TEST(ParseFloat, OverflowIsHugeValf) {
  EXPECT_FALSE(parse_number<float>("1e39").has_value());
  EXPECT_FALSE(parse_number<float>("3.40282357e38").has_value());
  EXPECT_FALSE(parse_number<float>("1e99999").has_value());
  EXPECT_EQ(bits(parse_number<float>("-1e39").value()), 0xFF800000u);
  EXPECT_EQ(bits(parse_number<float>("3.4028235e38").value()), 0x7F7FFFFFu);
}

TEST(StrToFloat, RandomDecimalsMatchStrtof) {
  std::mt19937_64 rng(1);  // NOLINT(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed) reproducible
  for (int i = 0; i < 50000; i++) {
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
    expect_str_to_float_matches(str.c_str());
  }
}

// Every 16411th float, printed with 6 to 17 digits, and the exact midpoint to the next float
TEST(StrToFloat, FloatsAndMidpointsMatchStrtof) {
  char buf[64];
  for (uint32_t u = 0; u < 0x7F800000; u += 16411) {
    for (int precision : {6, 9, 17}) {
      snprintf(buf, sizeof(buf), "%.*e", precision - 1, static_cast<double>(float_from(u)));
      expect_str_to_float_matches(buf);
    }
    expect_str_to_float_matches(midpoint_string(u).c_str());
  }
}

// Just above and just below a float midpoint, closer than half a double apart: newlib's strtod rounds to the
// midpoint and its float conversion then to the even neighbour; the parser rounds straight to the nearest float.
TEST(StrToFloat, NoDoubleRoundingNearMidpoints) {
  for (uint32_t u = 1; u < 0x7F800000; u += 65537) {
    expect_str_to_float_matches(midpoint_with_suffix(u, "0000001").c_str());
    expect_str_to_float_matches(midpoint_with_suffix(u, "9999999", true).c_str());
  }
}

TEST(StrToFloat, SyntaxMatchesNewlib) {
  static const char *const TOKENS[] = {"0", "1", "9", ".",  "e", "E",   "+",   "-",     "i",  "n", "f", "a",
                                       "N", "I", " ", "\t", "y", "inf", "nan", "inity", "00", "#", ""};
  for (const char *a : TOKENS) {
    for (const char *b : TOKENS) {
      for (const char *c : TOKENS) {
        for (const char *d : TOKENS) {
          const std::string str = std::string(a) + b + c + d;
          expect_str_to_float_matches(str.c_str());
        }
      }
    }
  }
}

// Ends taken from newlib's strtof (newlib-xtensa d6e3fcdb, the ESP8266 toolchain's), values correctly rounded
TEST(StrToFloat, SyntaxMatchesNewlibWhereLibcsDiffer) {
  struct Case {
    const char *str;
    uint32_t bits;
    int32_t end;
  };
  static const Case CASES[] = {
      // Above a float midpoint: newlib's gethex rounds these to the midpoint double, then to the even float
      {"0x1.100b0d0000000c", 0x3F880587, 18},
      {"0x1.0011010000000c", 0x3F800881, 18},
      {"0x1.0101010000000c0", 0x3F808081, 19},
      {"0x1.00000180000000001p0", 0x3F800001, 23},
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
  };
  for (const Case &c : CASES) {
    char *end = nullptr;
    const float value = str_to_float(c.str, &end);
    EXPECT_EQ(bits(value), c.bits) << c.str;
    EXPECT_EQ(end - c.str, c.end) << c.str;
  }
}

// The three inputs where newlib's strtof (double rounding) and picolibc's (correct rounding) were found to differ;
// every platform now gives picolibc's result
TEST(StrToFloat, RoundsCorrectlyWhereNewlibAndPicolibcDiffer) {
  const std::string long_tie = "1.000000059604644775390625" + std::string(800, '0') + "1";
  EXPECT_EQ(bits(str_to_float("1.0000000596046448", nullptr)), 0x3F800001u);
  EXPECT_EQ(bits(str_to_float("0x1.100b0d0000000c", nullptr)), 0x3F880587u);
  EXPECT_EQ(bits(str_to_float(long_tie.c_str(), nullptr)), 0x3F800001u);
  // Exactly halfway, with and without trailing zeros: ties to even
  EXPECT_EQ(bits(str_to_float("1.000000059604644775390625", nullptr)), 0x3F800000u);
  EXPECT_EQ(bits(str_to_float("1.000000178813934326171875000", nullptr)), 0x3F800002u);
  EXPECT_EQ(bits(str_to_float("0x1.000001p0", nullptr)), 0x3F800000u);
  EXPECT_EQ(bits(str_to_float("0x1.000003p0", nullptr)), 0x3F800002u);
}

// picolibc rounds a decimal to 9 digits before converting it, so these come out one float too high there; checked
// with exact rational arithmetic
TEST(StrToFloat, RoundsCorrectlyPastNineDigits) {
  EXPECT_EQ(bits(str_to_float("1.0856183295439e10", nullptr)), 0x5021C50Du);
  EXPECT_EQ(bits(str_to_float("3.97496754550e-4", nullptr)), 0x39D0671Cu);
  EXPECT_EQ(bits(str_to_float("9.76178673657108930e23", nullptr)), 0x674EB6C0u);
  EXPECT_EQ(bits(str_to_float("156.6562236e-26", nullptr)), 0x17F269ECu);
}

// RP2350 flushes float denormals in its double to float conversion; #22677 bench. The parser builds the bits
// itself, so a flushing conversion cannot leak into the result: every denormal, and every midpoint between two,
// converts by its own value.
TEST(StrToFloat, DenormalConversionDoesNotFlush) {
  const auto check = [](uint32_t k) {
    EXPECT_EQ(internal::double_to_float_bits(std::ldexp(static_cast<double>(k), -149)), k);
    // Halfway to the next one goes to the even one
    EXPECT_EQ(internal::double_to_float_bits(std::ldexp(k + 0.5, -149)), (k & 1) != 0 ? k + 1 : k);
  };
  for (uint32_t k = 0; k <= 0x800000; k += 7)
    check(k);
  // The largest denormal and FLT_MIN, where rounding carries into the exponent
  for (uint32_t k : {0x7FFFFEu, 0x7FFFFFu, 0x800000u})
    check(k);
}

// RP2350 bench: libc returned 0 and the first parser stalled stepping up from a flushed guess; #22677 bench
TEST(StrToFloat, DenormalsAndFltMinMatchStrtof) {
  for (const char *str : {"1.4e-45", "-1.4e-45", "1e-40", "1e-38", "1.17549435e-38", "1.1754942e-38", "1.17549421e-38",
                          "1.1754945e-38"}) {
    expect_str_to_float_matches(str);
  }
  for (uint32_t k = 1; k < 0x800000; k += 997)
    expect_str_to_float_matches(midpoint_string(k).c_str());
}

// The exact comparison with 5^n for n >= 10 overflowed in a draft; caught by the libc sweep
TEST(StrToFloat, ExactComparisonWithLargePowerOfFive) {
  for (const char *str : {"7.0064923216240854e-46", "7.0064923216240850e-46", "1.0003204120044314781215817e-39",
                          "1.0017175065733633207412926e-39"}) {
    expect_str_to_float_matches(str);
  }
}

// Home Assistant's non-numeric states; CodSpeed HomeassistantSensor_StateResponse_Unavailable on #22677
TEST(StrToFloat, NonNumericStatesAreRejected) {
  for (const char *str : {"unavailable", "unknown", "on", "off", "", "-", ".", "none", "inactive"}) {
    expect_str_to_float_matches(str);
    EXPECT_FALSE(parse_number<float>(str).has_value()) << str;
  }
}

// Python's repr of a sum, as Home Assistant sends: far from any float midpoint, so decided without big numbers;
// #22677 review
TEST(StrToFloat, PythonReprValuesTakeTheEarlyReturn) {
  for (const char *str : {"21.299999999999997", "0.30000000000000004", "0.1000000000000000055511151231257827"})
    expect_str_to_float_matches(str, 0);
}

// At, one double below and one double above a float midpoint: the early return must not decide these; #22677
// review
TEST(StrToFloat, MidpointNeighboursMatchStrtof) {
  char buf[200];
  for (uint32_t k : {0x3F800000u, 0x00000001u, 0x007FFFFFu, 0x7F7FFFFEu, 0x4CEB79A3u}) {
    const double mid = (static_cast<double>(float_from(k)) + float_from(k + 1)) / 2;
    for (double value : {mid, std::nextafter(mid, 0.0), std::nextafter(mid, INFINITY)}) {
      snprintf(buf, sizeof(buf), "%.120e", value);
      expect_str_to_float_matches(buf);
    }
  }
}

// The one float operation path takes up to 2^24 - 1 and 10^10; #22677 review
TEST(StrToFloat, FloatFastPathBoundaries) {
  for (const char *top : {"16777215", "16777216", "16777217"}) {
    for (const char *exp : {"e10", "e-10", "e11", "e-11", "e9", "e-9"}) {
      const std::string str = std::string(top) + exp;
      expect_str_to_float_matches(str.c_str());
    }
  }
}

// One double operation is exact up to 2^53 and 10^22; review
TEST(StrToFloat, DoubleFastPathBoundaries) {
  for (const char *top : {"9007199254740991", "9007199254740992", "9007199254740993"}) {
    for (const char *exp : {"e22", "e-22", "e23", "e-23"}) {
      const std::string str = std::string(top) + exp;
      expect_str_to_float_matches(str.c_str());
    }
  }
}

// The midpoint between FLT_MAX and 2^128 rounds to infinity; review
TEST(StrToFloat, OverflowMidpoint) {
  for (const char *str :
       {"340282356779733661637539395458142568448", "340282356779733661637539395458142568447.9",
        "340282356779733661637539395458142568448.1", "3.4028235677973366e38", "3.4028235677973365e38"}) {
    expect_str_to_float_matches(str);
  }
  EXPECT_EQ(bits(str_to_float("0x1.ffffffp127", nullptr)), 0x7F800000u);
}

// Exponents newlib caps at 19999, and one with more digits than any integer holds
TEST(StrToFloat, HugeExponents) {
  for (const char *str : {"1e-99999", "1e99999", "-1e99999", "0e99999", "1e9999999999999999999",
                          "1e-9999999999999999999", "1e-999999999999", "0.000e99999"}) {
    expect_str_to_float_matches(str);
  }
}

TEST(StrToFloat, LongMantissas) {
  std::string str = "1";
  str.append(799, '0');
  expect_str_to_float_matches((str + "e-799").c_str());
  std::mt19937_64 rng(2);  // NOLINT(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed) reproducible
  for (int i = 0; i < 50; i++) {
    str.clear();
    str += static_cast<char>('1' + rng() % 9);
    for (int j = 1; j < 800; j++)
      str += static_cast<char>('0' + rng() % 10);
    char exp[16];
    snprintf(exp, sizeof(exp), "e%d", static_cast<int>(rng() % 90) - 845);
    expect_str_to_float_matches((str + exp).c_str());
  }
  // 1 + 2^-24 exactly, 800 more zeros and a final 1: above the midpoint by far less than a double
  expect_str_to_float_matches(midpoint_with_suffix(0x3F800000, std::string(800, '0') + "1").c_str());
  str = "1.";
  str.append(3000, '0');
  expect_str_to_float_matches((str + "1e-3001").c_str());
  str = "0." + std::string(400, '0') + "34028235" + std::string(500, '7') + "e440";
  expect_str_to_float_matches(str.c_str());
  expect_str_to_float_matches((std::string(300, '9') + "e-261").c_str());
}

TEST(StrToFloat, HexDenormalsAndOverflow) {
  struct Case {
    const char *str;
    uint32_t bits;
  };
  static const Case CASES[] = {
      {"0x1p-149", 0x00000001},        {"0x1p-150", 0x00000000},
      {"0x1.8p-149", 0x00000002},      {"0x1.fffffcp-127", 0x007FFFFF},
      {"0x1.fffffep-127", 0x00800000}, {"-0x1p-149", 0x80000001},
      {"0x1p128", 0x7F800000},         {"0x1.fffffep127", 0x7F7FFFFF},
      {"0x1.fffffefp127", 0x7F7FFFFF}, {"0x1.0000000000001p-150", 0x00000001},
  };
  for (const Case &c : CASES)
    EXPECT_EQ(bits(str_to_float(c.str, nullptr)), c.bits) << c.str;
}

// newlib wraps the binary exponent at 32 bits; the parser saturates, so these stay zero and infinity
TEST(StrToFloat, HexExponentSaturates) {
  EXPECT_EQ(bits(str_to_float("0x1p-99999999999", nullptr)), 0x00000000u);
  EXPECT_EQ(bits(str_to_float("0x1p99999999999", nullptr)), 0x7F800000u);
  EXPECT_EQ(bits(str_to_float("0x1p4294967296", nullptr)), 0x7F800000u);
  EXPECT_EQ(bits(str_to_float("0x0.0000001p4294967300", nullptr)), 0x7F800000u);
}

}  // namespace esphome::core::testing
#endif  // USE_HOST
