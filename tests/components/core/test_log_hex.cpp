#include <gtest/gtest.h>

#include <cstdio>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::testing {

static const char *const TAG = "log_hex_test";

TEST(LogHexTest, EvaluatesArgumentsOnlyWhenTheLevelIsCompiledIn) {
  static constexpr uint8_t DATA[] = {0x01, 0xAB, 0xFF};
  int debug_calls = 0;
  int verbose_calls = 0;
  int very_verbose_calls = 0;
  auto count = [](int &calls) {
    calls++;
    return sizeof(DATA);
  };
  ESP_LOGD_HEX(TAG, DATA, count(debug_calls), sizeof(DATA), "Data: %s");
  ESP_LOGV_HEX(TAG, DATA, count(verbose_calls), sizeof(DATA), "Frame %u: %s", 7u);
  ESP_LOGVV_HEX(TAG, DATA, count(very_verbose_calls), 2, "Type %02X %s: %s", 0x10, "truncated");
#ifdef ESPHOME_LOG_HAS_DEBUG
  EXPECT_EQ(debug_calls, 1);
#else
  EXPECT_EQ(debug_calls, 0);
#endif
#ifdef ESPHOME_LOG_HAS_VERBOSE
  EXPECT_EQ(verbose_calls, 1);
#else
  EXPECT_EQ(verbose_calls, 0);
#endif
#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  EXPECT_EQ(very_verbose_calls, 1);
#else
  EXPECT_EQ(very_verbose_calls, 0);
#endif
}

TEST(LogHexTest, IsASingleStatement) {
  static constexpr uint8_t DATA[] = {0x12, 0x34};
  bool other_branch = false;
  if (DATA[0] == 0x12)  // NOLINT(readability-braces-around-statements)
    ESP_LOGV_HEX(TAG, DATA, sizeof(DATA), sizeof(DATA), "Data: %s");
  else  // NOLINT(readability-braces-around-statements)
    other_branch = true;
  EXPECT_FALSE(other_branch);
}

// Stands in for a log macro so the expansion can be checked without the logger backend
static char captured[64];  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#define CAPTURE_LOG(tag, ...) snprintf(captured, sizeof(captured), __VA_ARGS__)

TEST(LogHexTest, HexTextIsTheLastArgument) {
  static constexpr uint8_t DATA[] = {0x01, 0xAB, 0xFF};
  ESPHOME_LOG_HEX_(CAPTURE_LOG, TAG, DATA, sizeof(DATA), sizeof(DATA), "Data: %s");
  EXPECT_STREQ(captured, "Data: 01:AB:FF");
}

TEST(LogHexTest, ExtraArgumentsComeFirstAndLongDataIsCut) {
  static constexpr uint8_t DATA[] = {0x01, 0xAB, 0xFF};
  ESPHOME_LOG_HEX_(CAPTURE_LOG, TAG, DATA, sizeof(DATA), 2, "Type %02X %s: %s", 0x10, "truncated");
  EXPECT_STREQ(captured, "Type 10 truncated: 01:AB");
}

}  // namespace esphome::testing
