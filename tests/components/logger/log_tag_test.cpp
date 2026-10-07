#include <gtest/gtest.h>
#include <cstring>
#include <string>

#include "esphome/components/logger/log_buffer.h"
#include "esphome/core/log.h"

namespace esphome::logger::testing {

ESPHOME_LOG_TAG(TEST_TAG, "test.tag");

TEST(LogTagTest, TagReadsAsAString) { EXPECT_STREQ(TEST_TAG, "test.tag"); }

TEST(LogTagTest, HeaderContainsTagAndLine) {
  char data[256];
  LogBuffer buf{data, sizeof(data)};
  buf.write_header(5, TEST_TAG, 42, nullptr);
  const std::string header(data, buf.pos);
  EXPECT_NE(header.find("[D][test.tag:042]"), std::string::npos) << header;
}

}  // namespace esphome::logger::testing
