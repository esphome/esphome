#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/split_buffer/split_buffer.h"

namespace esphome::split_buffer::testing {

static std::vector<uint8_t> make_pattern(size_t length, uint8_t seed = 1) {
  std::vector<uint8_t> data(length);
  for (size_t i = 0; i != length; i++)
    data[i] = static_cast<uint8_t>(seed + i);
  return data;
}

static std::vector<uint8_t> read_all(const SplitBuffer &buffer) {
  std::vector<uint8_t> out(buffer.size());
  for (size_t i = 0; i != buffer.size(); i++)
    out[i] = buffer[i];
  return out;
}

TEST(SplitBufferInit, SingleBufferByDefault) {
  SplitBuffer buffer;
  ASSERT_TRUE(buffer.init(100));
  EXPECT_TRUE(buffer.is_valid());
  EXPECT_EQ(buffer.size(), 100u);
  EXPECT_EQ(buffer.get_buffer_count(), 1u);
}

TEST(SplitBufferInit, MaxBufferSizeSplits) {
  SplitBuffer buffer;
  ASSERT_TRUE(buffer.init(100, 32));
  EXPECT_EQ(buffer.size(), 100u);
  EXPECT_EQ(buffer.get_buffer_count(), 4u);
}

TEST(SplitBufferInit, ZeroLengthOrMaxFails) {
  SplitBuffer buffer;
  EXPECT_FALSE(buffer.init(0));
  EXPECT_FALSE(buffer.init(100, 0));
  EXPECT_FALSE(buffer.is_valid());
}

TEST(SplitBufferInit, StartsZeroed) {
  SplitBuffer buffer;
  ASSERT_TRUE(buffer.init(50, 16));
  EXPECT_EQ(read_all(buffer), std::vector<uint8_t>(50, 0));
}

TEST(SplitBufferFill, FillsShortLastBuffer) {
  SplitBuffer buffer;
  ASSERT_TRUE(buffer.init(50, 16));
  buffer.fill(0xA5);
  EXPECT_EQ(read_all(buffer), std::vector<uint8_t>(50, 0xA5));
}

TEST(SplitBufferGetSpan, SingleBufferCoversRest) {
  SplitBuffer buffer;
  ASSERT_TRUE(buffer.init(100));
  size_t length = 0;
  EXPECT_EQ(buffer.get_span(0, length), &buffer[0]);
  EXPECT_EQ(length, 100u);
  EXPECT_EQ(buffer.get_span(99, length), &buffer[99]);
  EXPECT_EQ(length, 1u);
}

TEST(SplitBufferGetSpan, StopsAtSubBufferBoundary) {
  SplitBuffer buffer;
  ASSERT_TRUE(buffer.init(50, 16));
  size_t length = 0;
  EXPECT_EQ(buffer.get_span(0, length), &buffer[0]);
  EXPECT_EQ(length, 16u);
  EXPECT_EQ(buffer.get_span(10, length), &buffer[10]);
  EXPECT_EQ(length, 6u);
  EXPECT_EQ(buffer.get_span(15, length), &buffer[15]);
  EXPECT_EQ(length, 1u);
  EXPECT_EQ(buffer.get_span(16, length), &buffer[16]);
  EXPECT_EQ(length, 16u);
}

TEST(SplitBufferGetSpan, ShortLastBuffer) {
  SplitBuffer buffer;
  ASSERT_TRUE(buffer.init(50, 16));
  size_t length = 0;
  EXPECT_EQ(buffer.get_span(48, length), &buffer[48]);
  EXPECT_EQ(length, 2u);
  EXPECT_EQ(buffer.get_span(49, length), &buffer[49]);
  EXPECT_EQ(length, 1u);
}

TEST(SplitBufferGetSpan, OutOfRange) {
  SplitBuffer buffer;
  ASSERT_TRUE(buffer.init(50, 16));
  size_t length = 99;
  EXPECT_EQ(buffer.get_span(50, length), nullptr);
  EXPECT_EQ(length, 0u);
  length = 99;
  EXPECT_EQ(buffer.get_span(1000, length), nullptr);
  EXPECT_EQ(length, 0u);
}

TEST(SplitBufferGetSpan, UninitializedReturnsNull) {
  SplitBuffer buffer;
  size_t length = 99;
  EXPECT_EQ(buffer.get_span(0, length), nullptr);
  EXPECT_EQ(length, 0u);
}

TEST(SplitBufferGetSpan, SpansCoverWholeBuffer) {
  SplitBuffer buffer;
  ASSERT_TRUE(buffer.init(50, 16));
  size_t index = 0;
  std::vector<size_t> lengths;
  while (index < buffer.size()) {
    size_t length = 0;
    ASSERT_NE(buffer.get_span(index, length), nullptr);
    lengths.push_back(length);
    index += length;
  }
  EXPECT_EQ(index, 50u);
  EXPECT_EQ(lengths, (std::vector<size_t>{16, 16, 16, 2}));
}

TEST(SplitBufferWrite, WithinOneSubBuffer) {
  SplitBuffer buffer;
  ASSERT_TRUE(buffer.init(50, 16));
  const auto data = make_pattern(5);
  buffer.write(3, data.data(), data.size());
  auto expected = std::vector<uint8_t>(50, 0);
  std::copy(data.begin(), data.end(), expected.begin() + 3);
  EXPECT_EQ(read_all(buffer), expected);
}

TEST(SplitBufferWrite, AcrossSubBuffers) {
  SplitBuffer buffer;
  ASSERT_TRUE(buffer.init(50, 16));
  const auto data = make_pattern(30);
  buffer.write(10, data.data(), data.size());
  auto expected = std::vector<uint8_t>(50, 0);
  std::copy(data.begin(), data.end(), expected.begin() + 10);
  EXPECT_EQ(read_all(buffer), expected);
}

TEST(SplitBufferWrite, WholeBuffer) {
  SplitBuffer buffer;
  ASSERT_TRUE(buffer.init(50, 16));
  const auto data = make_pattern(50);
  buffer.write(0, data.data(), data.size());
  EXPECT_EQ(read_all(buffer), data);
}

TEST(SplitBufferWrite, TruncatesPastEnd) {
  SplitBuffer buffer;
  ASSERT_TRUE(buffer.init(50, 16));
  // The source is sized to the full request so ASan catches any read beyond it.
  const auto data = make_pattern(20);
  buffer.write(40, data.data(), data.size());
  auto expected = std::vector<uint8_t>(50, 0);
  std::copy(data.begin(), data.begin() + 10, expected.begin() + 40);
  EXPECT_EQ(read_all(buffer), expected);
}

TEST(SplitBufferWrite, StartOutOfRangeIsIgnored) {
  SplitBuffer buffer;
  ASSERT_TRUE(buffer.init(50, 16));
  const auto data = make_pattern(5);
  buffer.write(50, data.data(), data.size());
  EXPECT_EQ(read_all(buffer), std::vector<uint8_t>(50, 0));
}

TEST(SplitBufferWrite, ZeroLengthIsNoop) {
  SplitBuffer buffer;
  ASSERT_TRUE(buffer.init(50, 16));
  buffer.write(0, nullptr, 0);
  EXPECT_EQ(read_all(buffer), std::vector<uint8_t>(50, 0));
}

TEST(SplitBufferWrite, MatchesContiguousBuffer) {
  // Each sub-buffer size, including ones that divide the total evenly, must give the same result.
  const auto data = make_pattern(64, 7);
  for (size_t max_size : {1u, 3u, 8u, 16u, 63u, 64u, 1000u}) {
    SplitBuffer buffer;
    ASSERT_TRUE(buffer.init(64, max_size));
    buffer.write(0, data.data(), 20);
    buffer.write(20, data.data() + 20, 44);
    EXPECT_EQ(read_all(buffer), data) << "max_buffer_size=" << max_size;
  }
}

}  // namespace esphome::split_buffer::testing
