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

using Fixed = StaticVector<uint8_t, 2>;

static Fixed pair(int first) { return {static_cast<uint8_t>(first), 9}; }

TEST(TemplatableBytesTest, FixedBufferLambda) {
  TemplatableBuffer<Fixed, int> bytes;
  bytes.set_template(pair);
  std::vector<uint8_t> seen;
  bytes.visit([&](const uint8_t *data, size_t len) { seen.assign(data, data + len); }, 4);
  EXPECT_EQ(seen, (std::vector<uint8_t>{4, 9}));
}

TEST(TemplatableBytesTest, FixedBufferValueKeepsWhatFits) {
  TemplatableBuffer<Fixed> bytes;
  bytes.set_static(PAYLOAD, sizeof(PAYLOAD));
  const Fixed value = bytes.value();
  EXPECT_EQ(std::vector<uint8_t>(value.begin(), value.end()), (std::vector<uint8_t>{1, 2}));
}

template<typename... Ts> class FixedHolder {
  TEMPLATABLE_BUFFER(data, StaticVector<uint8_t, 2>)

  size_t size(const Ts &...x) const { return this->data_.value(x...).size(); }
};

TEST(TemplatableBytesTest, BufferMacroTakesTypeWithComma) {
  FixedHolder<int> holder;
  holder.set_data_template(pair);
  EXPECT_EQ(holder.size(1), 2u);
}

}  // namespace esphome::testing
