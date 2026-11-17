#include <gtest/gtest.h>
#include <map>
#include <string>

#include "esphome/components/logger/log_buffer.h"
#include "esphome/components/logger/logger.h"
#include "esphome/core/log.h"

namespace esphome::logger::testing {

ESPHOME_LOG_TAG(TEST_TAG, "test.tag");

TEST(LogTagTest, HeaderContainsTagAndLine) {
  char data[256];
  LogBuffer buf{data, sizeof(data)};
  buf.write_header(5, TEST_TAG, 42, nullptr);
  const std::string header(data, buf.pos);
  EXPECT_NE(header.find("[D][test.tag:042]"), std::string::npos) << header;
}

TEST(LogTagTest, HeaderWithThreadNameAndFourDigitLine) {
  char data[256];
  LogBuffer buf{data, sizeof(data)};
  buf.write_header(5, TEST_TAG, 1234, "task");
  EXPECT_EQ(std::string(data, buf.pos), "\033[0;36m[D][test.tag:1234]\033[1;31m[task]\033[0;36m: ");
}

TEST(LogTagTest, HeaderWithoutLevelHasNoColor) {
  char data[256];
  LogBuffer buf{data, sizeof(data)};
  buf.write_header(0, TEST_TAG, 7, nullptr);
  EXPECT_EQ(std::string(data, buf.pos), "[][test.tag:007]: ");
}

TEST(LogTagTest, PerTagLevelLookupMatchesByContent) {
  std::map<const char *, uint8_t, CStrCompare> levels{{"api", 1}, {"sensor", 2}, {"wifi", 3}};
  // Copies, so a match must compare content rather than pointers
  char api[] = "api", sensor[] = "sensor", wifi[] = "wifi", missing[] = "switch";
  EXPECT_EQ(levels.find(FlashTag{api})->second, 1);
  EXPECT_EQ(levels.find(FlashTag{sensor})->second, 2);
  EXPECT_EQ(levels.find(FlashTag{wifi})->second, 3);
  EXPECT_EQ(levels.find(FlashTag{missing}), levels.end());
}

}  // namespace esphome::logger::testing
