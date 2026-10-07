#include <gtest/gtest.h>
#include <vector>
#include "esphome/core/automation.h"

namespace esphome::testing {

static const uint8_t PAYLOAD[] = {1, 2, 3};

static std::vector<uint8_t> repeat(int count) { return std::vector<uint8_t>(count, 7); }

TEST(TemplatableBytesTest, VisitStaticTable) {
  TemplatableBytes<> bytes;
  bytes.set_static(PAYLOAD, sizeof(PAYLOAD));
  std::vector<uint8_t> seen;
  bytes.visit([&](const uint8_t *data, size_t len) { seen.assign(data, data + len); });
  EXPECT_EQ(seen, (std::vector<uint8_t>{1, 2, 3}));
}

TEST(TemplatableBytesTest, VisitWithLargerStackBuffer) {
  TemplatableBytes<> bytes;
  bytes.set_static(PAYLOAD, sizeof(PAYLOAD));
  size_t seen = 0;
  bytes.visit<256>([&](const uint8_t *, size_t len) { seen = len; });
  EXPECT_EQ(seen, sizeof(PAYLOAD));
}

TEST(TemplatableBytesTest, VisitEmptyStaticTable) {
  TemplatableBytes<> bytes;
  bytes.set_static(nullptr, 0);
  size_t seen = 1;
  bytes.visit([&](const uint8_t *, size_t len) { seen = len; });
  EXPECT_EQ(seen, 0u);
}

TEST(TemplatableBytesTest, VisitLambdaWithArgument) {
  TemplatableBytes<int> bytes;
  bytes.set_template(repeat);
  std::vector<uint8_t> seen;
  bytes.visit([&](const uint8_t *data, size_t len) { seen.assign(data, data + len); }, 4);
  EXPECT_EQ(seen, (std::vector<uint8_t>(4, 7)));
}

}  // namespace esphome::testing
