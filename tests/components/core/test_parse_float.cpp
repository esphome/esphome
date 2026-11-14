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

// picolibc rounds a decimal to 9 significant digits and converts those to the nearest float, so up to 9
// significant digits its result is the correctly rounded float the host's strtof (glibc, macOS) gives. Longer
// input and the syntax where libcs differ are checked against values taken from picolibc 1.8.10's own strtof.
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

// The same bits and end as the host's strtof; for decimals of up to 9 significant digits
static void expect_str_to_float_matches(const char *str) {
  char *expected_end = nullptr;
  const float expected = reference_strtof(str, &expected_end);
  char *end = nullptr;
  const float actual = str_to_float(str, &end);
  EXPECT_EQ(bits(actual), bits(expected)) << '"' << str << '"';
  EXPECT_EQ(end - str, expected_end - str) << '"' << str << '"';
}

struct PicolibcCase {
  const char *str;
  uint32_t bits;
  int32_t end;
};

// Bits and end from picolibc 1.8.10's strtof
static void expect_picolibc(const PicolibcCase &c) {
  char *end = nullptr;
  const float value = str_to_float(c.str, &end);
  EXPECT_EQ(bits(value), c.bits) << '"' << c.str << '"';
  EXPECT_EQ(end - c.str, c.end) << '"' << c.str << '"';
}

static void expect_picolibc(const std::string &str, uint32_t expected) {
  char *end = nullptr;
  EXPECT_EQ(bits(str_to_float(str.c_str(), &end)), expected) << str.substr(0, 40);
  EXPECT_EQ(static_cast<size_t>(end - str.c_str()), str.size()) << str.substr(0, 40);
}

static float float_from(uint32_t value) { return bit_cast<float>(value); }

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

// Eight-digit mantissas below 2^24 stay on the one-division path; the next digit past the limit falls back
TEST(ParseFloat, EightDigitPlainDecimalsMatchStrtof) {
  for (const char *str : {"12345678", "-12345678", "1234.5678", "0.12345678", "16777209", "1677720.9", "0.16777209",
                          "16777210", "16777215", "1677721.5", "99999999", "9999999.9", "16777209."}) {
    expect_matches_strtof(str);
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
  for (const char *str : {"", "-", "+", ".", "abc", "12abc", "1.2.3", "1,5", "12 ", "--1", "1e", "1e400", "inf",
                          "infinity", "infi", "nan(12 ab)", "nan(12"}) {
    EXPECT_FALSE(parse_number<float>(str).has_value()) << str;
  }
}

// Above the midpoint 1 + 2^-24 by less than half a double: newlib's strtof rounds to the midpoint and then to
// 1.0f; picolibc's 9 digits, 1.00000006, give the next float.
TEST(ParseFloat, FullPathRoundsLikePicolibc) {
  EXPECT_EQ(bits(parse_number<float>("1.0000000596046448").value()), 0x3F800001u);
  EXPECT_EQ(bits(parse_number<float>("-1.0000000596046448").value()), 0xBF800001u);
}

// picolibc takes "nan(" with letters, digits and '_' up to a ")", and nothing at all otherwise
TEST(ParseFloat, NanForms) {
  EXPECT_TRUE(std::isnan(parse_number<float>("nan(12)").value()));
  EXPECT_TRUE(std::isnan(parse_number<float>("nan(xyz_1)").value()));
  EXPECT_FALSE(parse_number<float>("nan(x y)").has_value());
}

// The callers' overflow check: a positive overflow is HUGE_VALF and rejected, a negative one is -inf and accepted
TEST(ParseFloat, OverflowIsHugeValf) {
  EXPECT_FALSE(parse_number<float>("1e39").has_value());
  EXPECT_FALSE(parse_number<float>("3.40282357e38").has_value());
  EXPECT_FALSE(parse_number<float>("1e99999").has_value());
  EXPECT_EQ(bits(parse_number<float>("-1e39").value()), 0xFF800000u);
  EXPECT_EQ(bits(parse_number<float>("3.4028235e38").value()), 0x7F7FFFFFu);
}

// Up to 9 significant digits, with leading and trailing zeros, a point anywhere and an exponent
TEST(StrToFloat, RandomDecimalsMatchStrtof) {
  std::mt19937_64 rng(1);  // NOLINT(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed) reproducible
  for (int i = 0; i < 50000; i++) {
    std::string str;
    if (rng() % 3 == 0)
      str += (rng() & 1) ? "-" : "+";
    const int zeros = static_cast<int>(rng() % 4);
    const int significant = 1 + static_cast<int>(rng() % 9);
    const int trailing = static_cast<int>(rng() % 4);
    std::string digits(zeros, '0');
    digits += static_cast<char>('1' + rng() % 9);
    for (int j = 1; j < significant; j++)
      digits += static_cast<char>('0' + rng() % 10);
    digits.append(trailing, '0');
    const int point = static_cast<int>(rng() % (digits.size() + 2)) - 1;
    for (int j = 0; j < static_cast<int>(digits.size()); j++) {
      if (j == point)
        str += '.';
      str += digits[j];
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

// Every 16411th float, printed with 6 and 9 digits
TEST(StrToFloat, FloatsMatchStrtof) {
  char buf[64];
  for (uint32_t u = 0; u < 0x7F800000; u += 16411) {
    for (int precision : {6, 9}) {
      snprintf(buf, sizeof(buf), "%.*e", precision - 1, static_cast<double>(float_from(u)));
      expect_str_to_float_matches(buf);
    }
  }
}

TEST(StrToFloat, SyntaxMatchesStrtof) {
  // Tokens whose combinations the libcs read alike: no "x", "nan(" or "infi"
  static const char *const TOKENS[] = {"0", "1", "9", ".", "e", "E", "+", "-", "a", "N", " ", "\t", "y", "00", "#", ""};
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

// Where newlib and glibc read the text differently from picolibc
TEST(StrToFloat, SyntaxMatchesPicolibc) {
  static const PicolibcCase CASES[] = {
      // "x" after any zero digits starts hex, even after a point or a second "0x"
      {"000x", 0x00000000, 4},
      {"0x", 0x00000000, 2},
      {"-0x", 0x80000000, 3},
      {"00x1", 0x3F800000, 4},
      {"0x0x1", 0x3F800000, 5},
      {"0.0x1", 0x3B800000, 5},
      {".x1", 0x00000000, 0},
      {"0x.p1", 0x00000000, 5},
      {"0x.8", 0x3F000000, 4},
      {"0X1.8", 0x3FC00000, 5},
      {"0x1P-1", 0x3F000000, 6},
      {"0x1p+", 0x3F800000, 3},
      {"1x5", 0x3F800000, 1},
      // "inf" stops before anything but "i"; past "infi" only "infinity" is a number
      {"inf", 0x7F800000, 3},
      {"-inf", 0xFF800000, 4},
      {"INFINITY", 0x7F800000, 8},
      {"+infi0", 0x00000000, 0},
      {"infi", 0x00000000, 0},
      {"infinit", 0x00000000, 0},
      // "nan(" takes letters, digits and '_' up to ")", and is no number without the ")"
      {"NaN", 0x7FC00000, 3},
      {"nan()", 0x7FC00000, 5},
      {"nan(xyz)", 0x7FC00000, 8},
      {"nan(_a1)", 0x7FC00000, 8},
      {"-nan(1)", 0xFFC00000, 7},
      {"nan( )", 0x00000000, 0},
      {"nan(12 ab)", 0x00000000, 0},
      {"nan(12", 0x00000000, 0},
      // An exponent needs a digit
      {"1e", 0x3F800000, 1},
      {"1e+", 0x3F800000, 1},
      {"1e-x", 0x3F800000, 1},
      {"\t 12", 0x41400000, 4},
      {"- 1", 0x00000000, 0},
      {"1.2.3", 0x3F99999A, 3},
  };
  for (const PicolibcCase &c : CASES)
    expect_picolibc(c);
}

// The inputs where newlib's strtof (decimal to double to float) and picolibc's were found to differ; every platform
// now gives picolibc's result, which is also the correctly rounded float for these
TEST(StrToFloat, RoundsLikePicolibcWhereNewlibDoubleRounds) {
  expect_picolibc({"1.0000000596046448", 0x3F800001, 18});
  expect_picolibc({"0x1.100b0d0000000c", 0x3F880587, 18});
  expect_picolibc("1.000000059604644775390625" + std::string(800, '0') + "1", 0x3F800001);
}

// picolibc rounds a decimal to 9 significant digits, ties to even, before converting it: past 9 digits its float can
// be one away from the nearest, and an exact tie between two floats can round up. Values from picolibc.
TEST(StrToFloat, NineDigitRoundingLikePicolibc) {
  static const PicolibcCase CASES[] = {
      {"1.0856183295439e10", 0x5021C50E, 18},          // nearest 0x5021C50D
      {"3.97496754550e-4", 0x39D0671D, 16},            // nearest 0x39D0671C
      {"9.76178673657108930e23", 0x674EB6C1, 22},      // nearest 0x674EB6C0
      {"156.6562236e-26", 0x17F269ED, 15},             // nearest 0x17F269EC
      {"1.000000059604644775390625", 0x3F800001, 26},  // the tie 1 + 2^-24: nearest-even 0x3F800000
      {"1.000000059", 0x3F800001, 11},                 // 1.00000006 after rounding
      {"7.0064923216240854e-46", 0x00000000, 22},      // 7.00649232e-46, below half the smallest float
      {"21.299999999999997", 0x41AA6666, 18},
      {"0.30000000000000004", 0x3E99999A, 19},
      {"0.1000000000000000055511151231257827", 0x3DCCCCCD, 36},
  };
  for (const PicolibcCase &c : CASES)
    expect_picolibc(c);
}

// Nine digits converted exactly: ties to even between floats, where the 64-bit product cannot decide alone
TEST(StrToFloat, NineDigitTiesGoToEven) {
  static const PicolibcCase CASES[] = {
      {"16777217", 0x4B800000, 8},  {"16777219", 0x4B800002, 8},    {"16777217.0", 0x4B800000, 10},
      {"8388608.5", 0x4B000000, 9}, {"8388609.5", 0x4B000002, 9},   {"3e10", 0x50DF8476, 4},
      {"5e10", 0x513A43B7, 4},      {"1.00000006", 0x3F800001, 10},
  };
  for (const PicolibcCase &c : CASES)
    expect_picolibc(c);
}

// RP2350 flushes float denormals in its double to float conversion; #22677 bench. The parser builds the bits
// with integer operations, so every denormal parses to its own value.
TEST(StrToFloat, DenormalsParseWithoutFlushing) {
  char buf[64];
  const auto check = [&buf](uint32_t k) {
    snprintf(buf, sizeof(buf), "%.8e", std::ldexp(static_cast<double>(k), -149));
    EXPECT_EQ(bits(str_to_float(buf, nullptr)), k) << buf;
  };
  for (uint32_t k = 0; k <= 0x800000; k += 4099)
    check(k);
  // The largest denormal and FLT_MIN, where rounding carries into the exponent
  for (uint32_t k : {1u, 2u, 0x7FFFFEu, 0x7FFFFFu, 0x800000u})
    check(k);
}

// Pico W and Pico 2 W bench, #22677: the RP2040 and RP2350 libcs flush float denormals to 0 in their double to
// float conversion, and the RP2040 one turns a NaN into +infinity
TEST(StrToFloat, Rp2040LibcFlushesAndNanToInfinity) {
  static const PicolibcCase CASES[] = {
      {"1e-40", 0x000116C2, 5},          {"1.4e-45", 0x00000001, 7},  {"1e-38", 0x006CE3EE, 5},
      {"1.1754942e-38", 0x007FFFFF, 13}, {"0x1p-149", 0x00000001, 8}, {"1.17549435e-38", 0x00800000, 14},
      {"nan(12)", 0x7FC00000, 7},        {"nan", 0x7FC00000, 3},
  };
  for (const PicolibcCase &c : CASES)
    expect_picolibc(c);
}

// RP2350 bench: libc returned 0 and the first parser stalled stepping up from a flushed guess; #22677 bench
TEST(StrToFloat, DenormalsAndFltMinMatchStrtof) {
  for (const char *str : {"1.4e-45", "-1.4e-45", "1e-40", "1e-38", "1.17549435e-38", "1.1754942e-38", "1.17549421e-38",
                          "1.1754945e-38", "7.0064923e-46", "7.0064924e-46"}) {
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

// The one float operation path takes up to 2^24 - 1 and 10^10; #22677 review
TEST(StrToFloat, PlainPathBoundaries) {
  for (const char *top : {"16777215", "16777216", "16777217"}) {
    for (const char *exp : {"e10", "e-10", "e11", "e-11", "e9", "e-9", ".5", ".25"}) {
      const std::string str = std::string(top) + exp;
      expect_str_to_float_matches(str.c_str());
    }
  }
}

// The midpoint between FLT_MAX and 2^128 rounds to infinity; review
TEST(StrToFloat, OverflowMidpoint) {
  expect_picolibc({"340282356779733661637539395458142568448", 0x7F800000, 39});
  expect_picolibc({"3.40282356e38", 0x7F7FFFFF, 13});
  expect_picolibc({"3.40282357e38", 0x7F800000, 13});
  EXPECT_EQ(bits(str_to_float("0x1.ffffffp127", nullptr)), 0x7F800000u);
}

// picolibc stops adding exponent digits at 224, so the digits of the mantissa can move it no further
TEST(StrToFloat, HugeExponents) {
  for (const char *str : {"1e-99999", "1e99999", "-1e99999", "0e99999", "1e9999999999999999999",
                          "1e-9999999999999999999", "1e-999999999999", "0.000e99999", "1e224", "1e2240", "1e-2240"}) {
    expect_str_to_float_matches(str);
  }
  // 10^-3000 * 10^3000 is 1, but the exponent counts only as 300
  expect_picolibc("0." + std::string(2999, '0') + "1e3000", 0x00000000);
  expect_picolibc("1" + std::string(300, '0') + "e-300", 0x3F800000);
}

TEST(StrToFloat, LongMantissas) {
  expect_picolibc("1" + std::string(799, '0') + "e-799", 0x3F800000);
  expect_picolibc("1." + std::string(3000, '0') + "1e-3001", 0x00000000);
  expect_picolibc("0." + std::string(400, '0') + "34028235" + std::string(500, '7') + "e440", 0x7F800000);
  expect_picolibc(std::string(300, '9') + "e-261", 0x7F800000);
  expect_picolibc("3" + std::string(799, '7') + "e-761", 0x7F800000);
}

TEST(StrToFloat, HexDenormalsAndOverflow) {
  struct Case {
    const char *str;
    uint32_t bits;
  };
  static const Case CASES[] = {
      {"0x1p-149", 0x00000001},         {"0x1p-150", 0x00000000},
      {"0x1.8p-149", 0x00000002},       {"0x1.fffffcp-127", 0x007FFFFF},
      {"0x1.fffffep-127", 0x00800000},  {"-0x1p-149", 0x80000001},
      {"0x1p128", 0x7F800000},          {"0x1.fffffep127", 0x7F7FFFFF},
      {"0x1.fffffefp127", 0x7F7FFFFF},  {"0x1.0000000000001p-150", 0x00000001},
      {"0x1.000001p0", 0x3F800000},     {"0x1.000003p0", 0x3F800002},
      {"0x1p-99999999999", 0x00000000}, {"0x1p99999999999", 0x7F800000},
  };
  for (const Case &c : CASES)
    EXPECT_EQ(bits(str_to_float(c.str, nullptr)), c.bits) << c.str;
}

}  // namespace esphome::core::testing
#endif  // USE_HOST
