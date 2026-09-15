#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/split_buffer/split_buffer.h"

namespace esphome::split_buffer::testing {

namespace {

// Lengths worth covering: the degenerate ones, an exact power of two, and the sizes a display driver actually asks
// for. 96000 is a 800x480 panel at four pixels to the byte, and sits between two powers of two - the case where the
// stride is larger than the buffer it describes.
constexpr size_t LENGTHS[] = {1, 2, 3, 7, 64, 100, 1024, 48000, 96000};

uint8_t pattern_at(size_t index) { return static_cast<uint8_t>(index * 31 + 7); }

// Walk the buffer the way a driver is meant to read a run of bytes: ask span() for a pointer, consume what it says
// is contiguous, ask again. A span that stopped short, overlapped, or ran past the end shows up here as a length
// mismatch or as wrong data.
std::vector<uint8_t> collect_via_span(const SplitBuffer &buffer) {
  std::vector<uint8_t> seen;
  size_t index = 0;
  while (index < buffer.size()) {
    size_t length = 0;
    const uint8_t *data = buffer.span(index, length);
    if (data == nullptr || length == 0)
      break;  // leaves seen short, which the caller asserts on
    seen.insert(seen.end(), data, data + length);
    index += length;
  }
  return seen;
}

}  // namespace

TEST(SplitBufferTest, SpanWalkSeesEveryByteInOrder) {
  for (const size_t length : LENGTHS) {
    SplitBuffer buffer;
    ASSERT_TRUE(buffer.init(length)) << "init failed for length " << length;
    for (size_t i = 0; i != length; i++)
      buffer[i] = pattern_at(i);

    const std::vector<uint8_t> seen = collect_via_span(buffer);
    ASSERT_EQ(seen.size(), length) << "span walk did not cover length " << length;
    for (size_t i = 0; i != length; i++)
      ASSERT_EQ(seen[i], pattern_at(i)) << "at byte " << i << " of " << length;
  }
}

// The stride is a power of two and so is usually larger than the buffer, which is exactly when an unclamped span
// would hand back more bytes than were allocated.
TEST(SplitBufferTest, SpanStopsAtTheEndOfTheBuffer) {
  for (const size_t length : LENGTHS) {
    SplitBuffer buffer;
    ASSERT_TRUE(buffer.init(length));
    size_t reported = 0;
    ASSERT_NE(buffer.span(0, reported), nullptr);
    EXPECT_LE(reported, length) << "span ran past the end for length " << length;

    size_t tail = 0;
    ASSERT_NE(buffer.span(length - 1, tail), nullptr);
    EXPECT_EQ(tail, 1u) << "span from the last byte should be one byte, length " << length;
  }
}

TEST(SplitBufferTest, SpanRejectsAnIndexPastTheEnd) {
  SplitBuffer buffer;
  ASSERT_TRUE(buffer.init(64));
  size_t length = 123;  // must be overwritten, not left alone
  EXPECT_EQ(buffer.span(64, length), nullptr);
  EXPECT_EQ(length, 0u);
}

TEST(SplitBufferTest, SpanAndIndexingAgree) {
  SplitBuffer buffer;
  ASSERT_TRUE(buffer.init(48000));
  for (size_t i = 0; i != buffer.size(); i++)
    buffer[i] = pattern_at(i);

  size_t index = 0;
  while (index < buffer.size()) {
    size_t length = 0;
    const uint8_t *data = buffer.span(index, length);
    ASSERT_NE(data, nullptr);
    ASSERT_GT(length, 0u);
    for (size_t offset = 0; offset != length; offset++)
      ASSERT_EQ(data[offset], buffer[index + offset]) << "at byte " << index + offset;
    index += length;
  }
}

// fill() memsets each sub-buffer, and the last one is sized from the stride. Getting that arithmetic wrong leaves a
// tail of the buffer untouched, which a full-screen clear would show as a stripe.
TEST(SplitBufferTest, FillCoversTheWholeBuffer) {
  for (const size_t length : LENGTHS) {
    SplitBuffer buffer;
    ASSERT_TRUE(buffer.init(length));
    for (size_t i = 0; i != length; i++)
      buffer[i] = 0x00;

    buffer.fill(0xA5);
    const std::vector<uint8_t> seen = collect_via_span(buffer);
    ASSERT_EQ(seen.size(), length);
    for (size_t i = 0; i != length; i++)
      ASSERT_EQ(seen[i], 0xA5) << "unfilled byte " << i << " of " << length;
  }
}

TEST(SplitBufferTest, ZeroLengthIsRefused) {
  SplitBuffer buffer;
  EXPECT_FALSE(buffer.init(0));
  EXPECT_FALSE(buffer.is_valid());
  EXPECT_EQ(buffer.size(), 0u);
}

TEST(SplitBufferTest, FreeLeavesTheBufferReusable) {
  SplitBuffer buffer;
  ASSERT_TRUE(buffer.init(1024));
  buffer.free();
  EXPECT_FALSE(buffer.is_valid());

  ASSERT_TRUE(buffer.init(4096)) << "a freed buffer should initialise again";
  EXPECT_EQ(buffer.size(), 4096u);
  size_t length = 0;
  EXPECT_NE(buffer.span(0, length), nullptr);
  EXPECT_EQ(length, 4096u);
}

}  // namespace esphome::split_buffer::testing
