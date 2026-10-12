#include <gtest/gtest.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>

#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"

namespace esphome::core::testing {

// Helper to call value_accuracy_to_buf and return as string
static std::string va_to_string(float value, int8_t accuracy_decimals) {
  char buf[VALUE_ACCURACY_MAX_LEN];
  std::span<char, VALUE_ACCURACY_MAX_LEN> sp(buf);
  size_t len = value_accuracy_to_buf(sp, value, accuracy_decimals);
  return std::string(buf, len);
}

// Helper: reference implementation using snprintf for comparison
static std::string va_reference(float value, int8_t accuracy_decimals) {
  // Replicate normalize_accuracy_decimals logic
  if (accuracy_decimals < 0) {
    float divisor;
    if (accuracy_decimals == -1) {
      divisor = 10.0f;
    } else if (accuracy_decimals == -2) {
      divisor = 100.0f;
    } else {
      divisor = pow10_int(-accuracy_decimals);
    }
    value = roundf(value / divisor) * divisor;
    accuracy_decimals = 0;
  }
  char buf[VALUE_ACCURACY_MAX_LEN];
  snprintf(buf, sizeof(buf), "%.*f", accuracy_decimals, value);
  return std::string(buf);
}

// --- Basic formatting ---

TEST(ValueAccuracyToBuf, ZeroDecimals) {
  EXPECT_EQ(va_to_string(23.456f, 0), "23");
  EXPECT_EQ(va_to_string(0.0f, 0), "0");
  EXPECT_EQ(va_to_string(100.0f, 0), "100");
  EXPECT_EQ(va_to_string(1.0f, 0), "1");
}

TEST(ValueAccuracyToBuf, OneDecimal) {
  EXPECT_EQ(va_to_string(23.456f, 1), "23.5");
  EXPECT_EQ(va_to_string(0.0f, 1), "0.0");
  EXPECT_EQ(va_to_string(1.05f, 1), va_reference(1.05f, 1));
}

TEST(ValueAccuracyToBuf, TwoDecimals) {
  EXPECT_EQ(va_to_string(23.456f, 2), "23.46");
  EXPECT_EQ(va_to_string(0.0f, 2), "0.00");
  EXPECT_EQ(va_to_string(1.005f, 2), va_reference(1.005f, 2));
}

TEST(ValueAccuracyToBuf, ThreeDecimals) {
  EXPECT_EQ(va_to_string(23.456f, 3), "23.456");
  EXPECT_EQ(va_to_string(0.0f, 3), "0.000");
}

// --- Negative values ---

TEST(ValueAccuracyToBuf, NegativeValues) {
  EXPECT_EQ(va_to_string(-23.456f, 2), "-23.46");
  EXPECT_EQ(va_to_string(-0.5f, 1), "-0.5");
  EXPECT_EQ(va_to_string(-100.0f, 0), "-100");
}

// --- Negative accuracy_decimals (rounding to tens/hundreds) ---

TEST(ValueAccuracyToBuf, NegativeAccuracy) {
  EXPECT_EQ(va_to_string(1234.0f, -1), va_reference(1234.0f, -1));
  EXPECT_EQ(va_to_string(1234.0f, -2), va_reference(1234.0f, -2));
  EXPECT_EQ(va_to_string(56.0f, -1), va_reference(56.0f, -1));
}

// --- Special float values ---

TEST(ValueAccuracyToBuf, NaN) {
  std::string result = va_to_string(NAN, 2);
  EXPECT_EQ(result, va_reference(NAN, 2));
}

TEST(ValueAccuracyToBuf, Infinity) {
  std::string result = va_to_string(INFINITY, 2);
  EXPECT_EQ(result, va_reference(INFINITY, 2));
}

TEST(ValueAccuracyToBuf, NegativeInfinity) {
  std::string result = va_to_string(-INFINITY, 2);
  EXPECT_EQ(result, va_reference(-INFINITY, 2));
}

// --- Edge cases ---

TEST(ValueAccuracyToBuf, VerySmallValues) {
  EXPECT_EQ(va_to_string(0.001f, 3), "0.001");
  EXPECT_EQ(va_to_string(0.001f, 2), "0.00");
  EXPECT_EQ(va_to_string(0.009f, 2), "0.01");
}

TEST(ValueAccuracyToBuf, LargeValues) {
  EXPECT_EQ(va_to_string(999999.0f, 0), va_reference(999999.0f, 0));
  EXPECT_EQ(va_to_string(1013.25f, 2), "1013.25");
}

TEST(ValueAccuracyToBuf, Rounding) {
  // 0.5 rounds up
  EXPECT_EQ(va_to_string(23.5f, 0), "24");
  EXPECT_EQ(va_to_string(23.45f, 1), "23.5");  // float: 23.45 -> 23.4 or 23.5
  EXPECT_EQ(va_to_string(23.45f, 1), va_reference(23.45f, 1));
}

// --- Match snprintf for a range of typical sensor values ---

TEST(ValueAccuracyToBuf, MatchesSnprintf) {
  float test_values[] = {0.0f, 1.0f, -1.0f, 23.456f, -23.456f, 100.0f, 0.1f, 0.01f, 99.99f, 1013.25f, -40.0f};
  int8_t test_accuracies[] = {0, 1, 2, 3};

  for (float value : test_values) {
    for (int8_t acc : test_accuracies) {
      EXPECT_EQ(va_to_string(value, acc), va_reference(value, acc))
          << "Mismatch for value=" << value << " accuracy=" << static_cast<int>(acc);
    }
  }
}

// --- Return value (length) ---

TEST(ValueAccuracyToBuf, ReturnsCorrectLength) {
  char buf[VALUE_ACCURACY_MAX_LEN];
  std::span<char, VALUE_ACCURACY_MAX_LEN> sp(buf);

  size_t len = value_accuracy_to_buf(sp, 23.456f, 2);
  EXPECT_EQ(len, 5u);  // "23.46"
  EXPECT_EQ(strlen(buf), len);

  len = value_accuracy_to_buf(sp, 0.0f, 0);
  EXPECT_EQ(len, 1u);  // "0"
  EXPECT_EQ(strlen(buf), len);

  len = value_accuracy_to_buf(sp, -100.0f, 1);
  EXPECT_EQ(len, 6u);  // "-100.0"
  EXPECT_EQ(strlen(buf), len);
}

TEST(ValueAccuracyToBuf, NegativeZero) {
  // Hand-rolled formatter must preserve snprintf's sign-of-zero behavior.
  EXPECT_EQ(va_to_string(-0.0f, 2), va_reference(-0.0f, 2));
  EXPECT_EQ(va_to_string(-0.0f, 0), va_reference(-0.0f, 0));
  // Tiny negative that rounds to zero at this precision must still render as "-0.00".
  EXPECT_EQ(va_to_string(-0.001f, 2), va_reference(-0.001f, 2));
}

TEST(ValueAccuracyToBuf, OverflowFallsBackToSnprintf) {
  // |value| * 10^acc must exceed UINT32_MAX to exercise the snprintf fallback path.
  EXPECT_EQ(va_to_string(1.0e7f, 3), va_reference(1.0e7f, 3));
  EXPECT_EQ(va_to_string(-1.0e7f, 3), va_reference(-1.0e7f, 3));
  EXPECT_EQ(va_to_string(5.0e9f, 0), va_reference(5.0e9f, 0));
}

// --- value_accuracy_with_uom_to_buf ---

static std::string va_uom_to_string(float value, int8_t accuracy_decimals, const char *uom) {
  char buf[VALUE_ACCURACY_MAX_LEN];
  std::span<char, VALUE_ACCURACY_MAX_LEN> sp(buf);
  StringRef ref(uom);
  size_t len = value_accuracy_with_uom_to_buf(sp, value, accuracy_decimals, ref);
  return std::string(buf, len);
}

static std::string va_uom_reference(float value, int8_t accuracy_decimals, const char *uom) {
  char buf[VALUE_ACCURACY_MAX_LEN];
  if (!uom || *uom == '\0') {
    snprintf(buf, sizeof(buf), "%.*f", accuracy_decimals, value);
  } else {
    snprintf(buf, sizeof(buf), "%.*f %s", accuracy_decimals, value, uom);
  }
  return std::string(buf);
}

TEST(ValueAccuracyWithUomToBuf, BasicWithUnit) {
  EXPECT_EQ(va_uom_to_string(23.456f, 2, "°C"), va_uom_reference(23.456f, 2, "°C"));
  EXPECT_EQ(va_uom_to_string(1013.25f, 2, "hPa"), va_uom_reference(1013.25f, 2, "hPa"));
  EXPECT_EQ(va_uom_to_string(-40.0f, 1, "°F"), va_uom_reference(-40.0f, 1, "°F"));
  EXPECT_EQ(va_uom_to_string(100.0f, 0, "%"), va_uom_reference(100.0f, 0, "%"));
}

TEST(ValueAccuracyWithUomToBuf, EmptyUnit) {
  EXPECT_EQ(va_uom_to_string(23.456f, 2, ""), "23.46");
  EXPECT_EQ(va_uom_to_string(0.0f, 1, ""), "0.0");
}

TEST(ValueAccuracyWithUomToBuf, ReturnsCorrectLength) {
  char buf[VALUE_ACCURACY_MAX_LEN];
  std::span<char, VALUE_ACCURACY_MAX_LEN> sp(buf);
  StringRef ref("°C");
  size_t len = value_accuracy_with_uom_to_buf(sp, 23.46f, 2, ref);
  EXPECT_EQ(strlen(buf), len);
  EXPECT_EQ(len, strlen("23.46 °C"));
}

TEST(ValueAccuracyWithUomToBuf, NearBufferLimitTruncates) {
  // Build a unit long enough that value + " " + unit exceeds VALUE_ACCURACY_MAX_LEN.
  // "23.46" (5) + " " (1) + unit -> must cap at buf.size()-1 and stay null-terminated.
  std::string long_unit(VALUE_ACCURACY_MAX_LEN, 'U');
  char buf[VALUE_ACCURACY_MAX_LEN];
  std::span<char, VALUE_ACCURACY_MAX_LEN> sp(buf);
  StringRef ref(long_unit.c_str());
  size_t len = value_accuracy_with_uom_to_buf(sp, 23.46f, 2, ref);
  EXPECT_LT(len, VALUE_ACCURACY_MAX_LEN);
  EXPECT_EQ(strlen(buf), len);
  // Should begin with the formatted value and a separator.
  EXPECT_EQ(std::string(buf, 6), "23.46 ");
}

TEST(ValueAccuracyWithUomToBuf, MatchesSnprintf) {
  const char *units[] = {"°C", "hPa", "%", "W", "kWh", "m/s"};
  float values[] = {0.0f, 23.456f, -40.0f, 1013.25f, 100.0f};
  int8_t accs[] = {0, 1, 2, 3};
  for (const char *u : units) {
    for (float v : values) {
      for (int8_t a : accs) {
        EXPECT_EQ(va_uom_to_string(v, a, u), va_uom_reference(v, a, u))
            << "value=" << v << " acc=" << static_cast<int>(a) << " uom=" << u;
      }
    }
  }
}

}  // namespace esphome::core::testing
