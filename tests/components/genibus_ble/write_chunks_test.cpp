#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <vector>

#include "esphome/components/genibus_ble/write_chunks.h"

namespace esphome::genibus_ble::testing {

namespace {

using Bytes = std::vector<uint8_t>;

std::vector<Bytes> split(const Bytes &data, size_t fail_at = SIZE_MAX, bool *ok = nullptr) {
  std::vector<Bytes> chunks;
  const bool result = write_in_chunks(data, 20, [&](std::span<const uint8_t> chunk) {
    if (chunks.size() == fail_at)
      return false;
    chunks.emplace_back(chunk.begin(), chunk.end());
    return true;
  });
  if (ok != nullptr)
    *ok = result;
  return chunks;
}

Bytes counting(size_t size) {
  Bytes data(size);
  for (size_t i = 0; i < size; i++)
    data[i] = static_cast<uint8_t>(i);
  return data;
}

}  // namespace

// The GET of 93/289 as the pump gets it
TEST(GenibusBleWriteChunks, ShortFrameIsOneWrite) {
  const Bytes frame = {0x27, 0x07, 0xE7, 0xF8, 0x0A, 0x03, 0x5D, 0x01, 0x21, 0x52, 0x1F};
  bool ok = false;
  const auto chunks = split(frame, SIZE_MAX, &ok);
  EXPECT_TRUE(ok);
  ASSERT_EQ(chunks.size(), 1u);
  EXPECT_EQ(chunks[0], frame);
}

TEST(GenibusBleWriteChunks, ExactlyOneChunk) {
  const auto chunks = split(counting(20));
  ASSERT_EQ(chunks.size(), 1u);
  EXPECT_EQ(chunks[0].size(), 20u);
}

// The largest frame the bus builds: MAX_REQUEST_SIZE plus 6 bytes of frame
TEST(GenibusBleWriteChunks, LongFrameIsSplitInOrder) {
  const Bytes frame = counting(38);
  const auto chunks = split(frame);
  ASSERT_EQ(chunks.size(), 2u);
  EXPECT_EQ(chunks[0], Bytes(frame.begin(), frame.begin() + 20));
  EXPECT_EQ(chunks[1], Bytes(frame.begin() + 20, frame.end()));
}

TEST(GenibusBleWriteChunks, StopsAtFirstFailure) {
  bool ok = true;
  const auto chunks = split(counting(45), 1, &ok);
  EXPECT_FALSE(ok);
  EXPECT_EQ(chunks.size(), 1u);
}

TEST(GenibusBleWriteChunks, EmptyWritesNothing) {
  bool ok = false;
  EXPECT_TRUE(split({}, SIZE_MAX, &ok).empty());
  EXPECT_TRUE(ok);
}

}  // namespace esphome::genibus_ble::testing
