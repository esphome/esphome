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
  const uint8_t *frame = nullptr;
  for (uint8_t c : {0xFF, 0x01, 0x02, 0x03}) {
    frame = reader.feed(c);
  }
  ASSERT_NE(frame, nullptr);
  reader.resync();
  EXPECT_EQ(reader.length(), 0u);
}

TEST(FrameReaderTest, ResyncKeepsPartialHeaderAtEnd) {
  FrameReader<5, 0x3C, 0x32> reader;
  const uint8_t *frame = nullptr;
  for (uint8_t c : {0x3C, 0x32, 0x00, 0x00, 0x3C}) {
    frame = reader.feed(c);
  }
  ASSERT_NE(frame, nullptr);
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

// Feeds bytes and collects every completed frame of a LengthFrameReader, taking each one with next().
template<size_t N, class... F> static Frames feed_all(LengthFrameReader<N, F...> &reader, const Bytes &input) {
  Frames frames;
  for (uint8_t c : input) {
    for (const uint8_t *frame = reader.feed(c); frame != nullptr; frame = reader.next())
      frames.emplace_back(frame, frame + reader.frame_length());
  }
  return frames;
}

// Feeds bytes and returns the last frame feed() completed, or nullptr.
template<size_t N, class... F> static const uint8_t *feed_last(LengthFrameReader<N, F...> &reader, const Bytes &input) {
  const uint8_t *last = nullptr;
  for (uint8_t c : input) {
    if (const uint8_t *frame = reader.feed(c))
      last = frame;
  }
  return last;
}

template<size_t N, class... F> static Bytes held(const LengthFrameReader<N, F...> &reader, const uint8_t *frame) {
  return Bytes(frame, frame + reader.frame_length());
}

// pioneer_wt: A5 01, one byte at 7 holds the whole length
using OneByte = FrameFormat<FrameLengthU8<7>, 0xA5, 0x01>;
// tuya: 55 AA, payload length big-endian at 4, header, length and checksum add 7
using TwoByteBE = FrameFormat<FrameLengthU16BE<4, 7>, 0x55, 0xAA>;
// ld24xx command and data frames: payload length little-endian at 4, header, length and footer add 10
using LdCmd = FrameFormat<FrameLengthU16LE<4, 10>, 0xFD, 0xFC, 0xFB, 0xFA>;
using LdData = FrameFormat<FrameLengthU16LE<4, 10>, 0xF4, 0xF3, 0xF2, 0xF1>;
// ld2410s short frame: 6E, always 5 bytes
using LdShort = FrameFormat<FrameFixedLength<5>, 0x6E>;
// Small formats for the recovery tests: A5, one byte at 1 holds the whole length
using Small = FrameFormat<FrameLengthU8<1>, 0xA5>;

static_assert(sizeof(LengthFrameReader<255, OneByte>) == 257);
static_assert(sizeof(LengthFrameReader<300, TwoByteBE>) == 304);

TEST(LengthFrameReaderTest, OneByteLength) {
  LengthFrameReader<16, OneByte> reader;
  EXPECT_EQ(feed_all(reader, {0x00, 0xA5, 0x01, 0x10, 0x11, 0x12, 0x13, 0x00, 0x09, 0x42}),
            (Frames{{0xA5, 0x01, 0x10, 0x11, 0x12, 0x13, 0x00, 0x09, 0x42}}));
}

TEST(LengthFrameReaderTest, TwoByteBigEndianPayloadLength) {
  LengthFrameReader<300, TwoByteBE> reader;
  // Payload of 0x0102 = 258 bytes: 265 in all
  Bytes input{0x55, 0xAA, 0x03, 0x07, 0x01, 0x02};
  input.resize(265, 0x33);
  Frames frames = feed_all(reader, input);
  ASSERT_EQ(frames.size(), 1u);
  EXPECT_EQ(frames[0].size(), 265u);
  // Too long for N: not a frame
  EXPECT_EQ(feed_all(reader, {0x55, 0xAA, 0x03, 0x07, 0x01, 0x30}), Frames{});
  EXPECT_EQ(reader.length(), 0u);
}

TEST(LengthFrameReaderTest, TwoFormatsPickedByFirstByte) {
  LengthFrameReader<32, LdCmd, LdData> reader;
  const Bytes cmd{0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xA0, 0x01, 0x04, 0x03, 0x02, 0x01};
  const Bytes data{0xF4, 0xF3, 0xF2, 0xF1, 0x01, 0x00, 0x55, 0xF8, 0xF7, 0xF6, 0xF5};
  Bytes input{0x00};
  input.insert(input.end(), data.begin(), data.end());
  input.insert(input.end(), cmd.begin(), cmd.end());
  EXPECT_EQ(feed_all(reader, input), (Frames{data, cmd}));
}

TEST(LengthFrameReaderTest, FixedAndLengthFormatsTogether) {
  LengthFrameReader<32, LdShort, LdData> reader;
  const Bytes small{0x6E, 0x01, 0x02, 0x03, 0x62};
  const Bytes data{0xF4, 0xF3, 0xF2, 0xF1, 0x00, 0x00, 0xF8, 0xF7, 0xF6, 0xF5};
  Bytes input = small;
  input.insert(input.end(), data.begin(), data.end());
  input.insert(input.end(), small.begin(), small.end());
  EXPECT_EQ(feed_all(reader, input), (Frames{small, data, small}));
}

TEST(LengthFrameReaderTest, BrokenHeaderRealigns) {
  LengthFrameReader<32, LdCmd, LdData> reader;
  // FD FC then F4 breaks the command header and starts a data header
  const Bytes data{0xF4, 0xF3, 0xF2, 0xF1, 0x00, 0x00, 0xF8, 0xF7, 0xF6, 0xF5};
  Bytes input{0xFD, 0xFC};
  input.insert(input.end(), data.begin(), data.end());
  EXPECT_EQ(feed_all(reader, input), (Frames{data}));
}

TEST(LengthFrameReaderTest, BadLengthRealignsAfterFirstByte) {
  LengthFrameReader<8, Small> reader;
  // 0x01 ends the frame before its length byte and 0x09 is longer than 8: neither is a frame.
  // The second A5 inside the bad one starts the real frame.
  EXPECT_EQ(feed_all(reader, {0xA5, 0x01, 0xA5, 0x09, 0xA5, 0x04, 0x01, 0x02}), (Frames{{0xA5, 0x04, 0x01, 0x02}}));
}

TEST(LengthFrameReaderTest, LengthByteCanStartHeader) {
  LengthFrameReader<8, FrameFormat<FrameLengthU8<1>, 0x09>> reader;
  // Length 9 is longer than 8, so the length byte becomes the next header
  EXPECT_EQ(feed_all(reader, {0x09, 0x09, 0x03, 0x07}), (Frames{{0x09, 0x03, 0x07}}));
}

TEST(LengthFrameReaderTest, LengthTracksPartialFrame) {
  LengthFrameReader<8, Small> reader;
  feed_all(reader, {0xA5, 0x05});
  EXPECT_EQ(reader.length(), 2u);
  reader.reset();
  EXPECT_EQ(reader.length(), 0u);
  EXPECT_EQ(feed_all(reader, {0x01, 0xA5, 0x03, 0x07}), (Frames{{0xA5, 0x03, 0x07}}));
}

TEST(LengthFrameReaderTest, ResyncFindsHeaderInRejectedFrame) {
  LengthFrameReader<8, Small> reader;
  // The A5 taken as a header was payload; the real frame starts at the second A5
  const uint8_t *frame = feed_last(reader, {0xA5, 0x05, 0x00, 0xA5, 0x04});
  ASSERT_NE(frame, nullptr);
  EXPECT_EQ(reader.frame_length(), 5u);
  EXPECT_EQ(reader.resync(), nullptr);
  EXPECT_EQ(reader.length(), 2u);
  EXPECT_EQ(feed_all(reader, {0x10, 0x20}), (Frames{{0xA5, 0x04, 0x10, 0x20}}));
}

TEST(LengthFrameReaderTest, ResyncReturnsEveryFrameInsideRejectedOne) {
  LengthFrameReader<16, Small> reader;
  // A bad checksum rejects the 9 byte frame; two complete frames sit inside it, and a partial one after them
  const uint8_t *frame = feed_last(reader, {0xA5, 0x09, 0xA5, 0x03, 0x01, 0xA5, 0x02, 0xA5, 0x04});
  ASSERT_NE(frame, nullptr);
  frame = reader.resync();
  ASSERT_NE(frame, nullptr);
  EXPECT_EQ(held(reader, frame), (Bytes{0xA5, 0x03, 0x01}));
  frame = reader.next();
  ASSERT_NE(frame, nullptr);
  EXPECT_EQ(held(reader, frame), (Bytes{0xA5, 0x02}));
  EXPECT_EQ(reader.next(), nullptr);
  EXPECT_EQ(reader.length(), 2u);
  EXPECT_EQ(feed_all(reader, {0x01, 0x02}), (Frames{{0xA5, 0x04, 0x01, 0x02}}));
}

TEST(LengthFrameReaderTest, ResyncAgainAfterRecoveredFrameIsRejected) {
  LengthFrameReader<16, Small> reader;
  // The 9 byte frame holds a 4 byte candidate, which holds a 3 byte one, followed by two more bytes
  const uint8_t *frame = feed_last(reader, {0xA5, 0x09, 0xA5, 0x04, 0xA5, 0x03, 0x01, 0x02, 0x03});
  ASSERT_NE(frame, nullptr);
  frame = reader.resync();
  ASSERT_NE(frame, nullptr);
  EXPECT_EQ(held(reader, frame), (Bytes{0xA5, 0x04, 0xA5, 0x03}));
  // Rejected too: the 3 byte frame inside it comes back, and the bytes after the 4 byte one are kept
  frame = reader.resync();
  ASSERT_NE(frame, nullptr);
  EXPECT_EQ(held(reader, frame), (Bytes{0xA5, 0x03, 0x01}));
  EXPECT_EQ(reader.next(), nullptr);
  EXPECT_EQ(reader.length(), 0u);
}

TEST(LengthFrameReaderTest, ResyncPartialFrameAfterSilence) {
  LengthFrameReader<16, Small> reader;
  // A corrupt length of 15 holds back two complete frames until the caller gives up on the partial one
  EXPECT_EQ(feed_all(reader, {0xA5, 0x0F, 0xA5, 0x04, 0x01, 0x02, 0xA5, 0x03, 0x09}), Frames{});
  EXPECT_EQ(reader.length(), 9u);
  Frames found;
  for (const uint8_t *frame = reader.resync(); frame != nullptr; frame = reader.next())
    found.push_back(held(reader, frame));
  EXPECT_EQ(found, (Frames{{0xA5, 0x04, 0x01, 0x02}, {0xA5, 0x03, 0x09}}));
  EXPECT_EQ(reader.length(), 0u);
}

TEST(LengthFrameReaderTest, ResyncWhileIdleIsHarmless) {
  LengthFrameReader<8, Small> reader;
  EXPECT_EQ(reader.resync(), nullptr);
  // A handled frame, then noise: nothing is held any more
  EXPECT_EQ(feed_all(reader, {0xA5, 0x03, 0x01, 0x00}), (Frames{{0xA5, 0x03, 0x01}}));
  EXPECT_EQ(reader.resync(), nullptr);
  // A rejected length leaves nothing collected
  EXPECT_EQ(feed_all(reader, {0xA5, 0x09}), Frames{});
  EXPECT_EQ(reader.resync(), nullptr);
  EXPECT_EQ(feed_all(reader, {0xA5, 0x03, 0x01}), (Frames{{0xA5, 0x03, 0x01}}));
}

TEST(LengthFrameReaderTest, SixteenBitIndex) {
  LengthFrameReader<300, FrameFormat<FrameLengthU8<1, 45>, 0xAA>> reader;
  Bytes input{0x00, 0xAA, 0xFF};
  input.resize(input.size() + 298, 0x42);
  Frames frames = feed_all(reader, input);
  ASSERT_EQ(frames.size(), 1u);
  EXPECT_EQ(frames[0].size(), 300u);
  EXPECT_EQ(frames[0][299], 0x42);
}

}  // namespace esphome::core::testing
