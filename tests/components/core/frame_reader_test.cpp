#include <gtest/gtest.h>
#include <vector>
#include "esphome/core/frame_reader.h"

namespace esphome::core::testing {

using Bytes = std::vector<uint8_t>;
using Frames = std::vector<Bytes>;

// Feeds bytes and collects every completed frame.
template<size_t N, uint8_t... H> static Frames feed_all(FrameReader<N, H...> &reader, const Bytes &input) {
  Frames frames;
  for (uint8_t c : input) {
    const uint8_t *frame = reader.feed(c);
    if (frame != nullptr) {
      EXPECT_EQ(reader.length(), 0u);
      frames.emplace_back(frame, frame + N);
    }
  }
  return frames;
}

static_assert(sizeof(FrameReader<4, 0xFF>) == 5);
static_assert(sizeof(FrameReader<9, 0x55, 0x01, 0x01, 0x04>) == 10);
static_assert(sizeof(FrameReader<0xFF, 0xAA>) == 0x100);
static_assert(sizeof(FrameReader<0x100, 0xAA>) == 0x102);

TEST(FrameReaderTest, SingleByteHeader) {
  FrameReader<4, 0xFF> reader;
  EXPECT_EQ(feed_all(reader, {0xFF, 0x01, 0x02, 0x02, 0xFF, 0x03, 0x04, 0x06}),
            (Frames{{0xFF, 0x01, 0x02, 0x02}, {0xFF, 0x03, 0x04, 0x06}}));
}

TEST(FrameReaderTest, SkipsBytesBeforeHeader) {
  FrameReader<3, 0xFF> reader;
  EXPECT_EQ(feed_all(reader, {0x01, 0x02, 0xFF, 0x10, 0x20}), (Frames{{0xFF, 0x10, 0x20}}));
}

TEST(FrameReaderTest, HeaderByteInsidePayloadIsData) {
  FrameReader<4, 0xFF> reader;
  EXPECT_EQ(feed_all(reader, {0xFF, 0xFF, 0xFF, 0x00}), (Frames{{0xFF, 0xFF, 0xFF, 0x00}}));
}

TEST(FrameReaderTest, LengthTracksPartialFrame) {
  FrameReader<4, 0xAA, 0xBB> reader;
  EXPECT_EQ(reader.length(), 0u);
  reader.feed(0x00);
  EXPECT_EQ(reader.length(), 0u);
  reader.feed(0xAA);
  EXPECT_EQ(reader.length(), 1u);
  reader.feed(0xBB);
  reader.feed(0x01);
  EXPECT_EQ(reader.length(), 3u);
  reader.reset();
  EXPECT_EQ(reader.length(), 0u);
  EXPECT_EQ(feed_all(reader, {0x02, 0xAA, 0xBB, 0x03, 0x04}), (Frames{{0xAA, 0xBB, 0x03, 0x04}}));
}

TEST(FrameReaderTest, MultiByteHeaderMismatchRestarts) {
  FrameReader<6, 0x55, 0x01, 0x01, 0x04> reader;
  EXPECT_EQ(feed_all(reader, {0x55, 0x01, 0x02, 0x55, 0x01, 0x01, 0x04, 0x11, 0x22}),
            (Frames{{0x55, 0x01, 0x01, 0x04, 0x11, 0x22}}));
}

TEST(FrameReaderTest, MismatchedByteCanStartHeader) {
  FrameReader<6, 0x55, 0x01, 0x01, 0x04> reader;
  // The second 0x55 breaks the first header and starts the real one
  EXPECT_EQ(feed_all(reader, {0x55, 0x01, 0x55, 0x01, 0x01, 0x04, 0x11, 0x22}),
            (Frames{{0x55, 0x01, 0x01, 0x04, 0x11, 0x22}}));
}

TEST(FrameReaderTest, RepeatedHeaderPrefixIsKept) {
  // AA AA AA BB: the mismatch at the third byte still leaves AA AA matched
  FrameReader<4, 0xAA, 0xAA, 0xBB> reader;
  EXPECT_EQ(feed_all(reader, {0xAA, 0xAA, 0xAA, 0xBB, 0x01}), (Frames{{0xAA, 0xAA, 0xBB, 0x01}}));
}

TEST(FrameReaderTest, OverlappingHeaderPrefix) {
  // A1 B2 A1 B2: the mismatch on the last B2 keeps the trailing A1 B2 as the start of the header
  FrameReader<5, 0xA1, 0xB2, 0xA1, 0xC3> reader;
  EXPECT_EQ(feed_all(reader, {0xA1, 0xB2, 0xA1, 0xB2, 0xA1, 0xC3, 0x07}), (Frames{{0xA1, 0xB2, 0xA1, 0xC3, 0x07}}));
}

TEST(FrameReaderTest, ResyncFindsHeaderInRejectedFrame) {
  FrameReader<4, 0xFF> reader;
  // Started mid-frame: the 0xFF taken as a header was payload; the real frame starts at the second 0xFF
  const uint8_t *frame = nullptr;
  for (uint8_t c : {0xFF, 0x12, 0xFF, 0x01}) {
    frame = reader.feed(c);
  }
  ASSERT_NE(frame, nullptr);
  reader.resync();
  EXPECT_EQ(reader.length(), 2u);
  EXPECT_EQ(feed_all(reader, {0x02, 0x02}), (Frames{{0xFF, 0x01, 0x02, 0x02}}));
}

TEST(FrameReaderTest, ResyncWithoutHeaderStartsOver) {
  FrameReader<4, 0xFF> reader;
  for (uint8_t c : {0xFF, 0x01, 0x02, 0x03}) {
    reader.feed(c);
  }
  reader.resync();
  EXPECT_EQ(reader.length(), 0u);
}

TEST(FrameReaderTest, ResyncKeepsPartialHeaderAtEnd) {
  FrameReader<5, 0x3C, 0x32> reader;
  for (uint8_t c : {0x3C, 0x32, 0x00, 0x00, 0x3C}) {
    reader.feed(c);
  }
  reader.resync();
  EXPECT_EQ(reader.length(), 1u);
  EXPECT_EQ(feed_all(reader, {0x32, 0x01, 0x02, 0x03}), (Frames{{0x3C, 0x32, 0x01, 0x02, 0x03}}));
}

TEST(FrameReaderTest, ResyncMultiByteHeader) {
  FrameReader<6, 0x3C, 0x32> reader;
  const uint8_t *frame = nullptr;
  for (uint8_t c : {0x3C, 0x32, 0x3C, 0x3C, 0x32, 0x09}) {
    frame = reader.feed(c);
  }
  ASSERT_NE(frame, nullptr);
  reader.resync();
  EXPECT_EQ(reader.length(), 3u);
  EXPECT_EQ(feed_all(reader, {0x0A, 0x0B, 0x0C}), (Frames{{0x3C, 0x32, 0x09, 0x0A, 0x0B, 0x0C}}));
}

TEST(FrameReaderTest, SixteenBitIndex) {
  FrameReader<300, 0xAA, 0x55> reader;
  Bytes input{0x00, 0xAA, 0x55};
  input.resize(input.size() + 298, 0x42);
  Frames frames = feed_all(reader, input);
  ASSERT_EQ(frames.size(), 1u);
  EXPECT_EQ(frames[0][0], 0xAA);
  EXPECT_EQ(frames[0][1], 0x55);
  EXPECT_EQ(frames[0][299], 0x42);
}

}  // namespace esphome::core::testing
