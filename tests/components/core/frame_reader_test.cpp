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

// Feeds bytes and collects every frame the reader hands over, rejecting those reject() picks.
template<size_t N, class... F, class R>
static Frames feed_all(LengthFrameReader<N, F...> &reader, const Bytes &input, R reject) {
  Frames frames;
  auto handle = [&](const uint8_t *frame, size_t len) {
    frames.emplace_back(frame, frame + len);
    return !reject(frames.back());
  };
  for (uint8_t c : input)
    reader.feed(c, handle);
  return frames;
}
template<size_t N, class... F> static Frames feed_all(LengthFrameReader<N, F...> &reader, const Bytes &input) {
  return feed_all(reader, input, [](const Bytes & /*frame*/) { return false; });
}

// Takes every frame and keeps a copy
struct Collect {
  Frames seen;
  bool operator()(const uint8_t *frame, size_t len) {
    this->seen.emplace_back(frame, frame + len);
    return true;
  }
};

// pioneer_wt: A5 01, one byte at 7 holds the whole length
using OneByte = FrameFormat<FrameHeader<0xA5, 0x01>, FrameLengthU8<LengthField{.offset = 7}>>;
// tuya: 55 AA, version, command, payload length (2, BE), payload, checksum
using TwoByteBE = FrameFormat<FrameHeader<0x55, 0xAA>, PayloadLengthU16BE<LengthField{.offset = 4, .trailer = 1}>>;
// ld24xx command and data frames: header (4), payload length (2, LE), payload, footer (4)
using LdCmd =
    FrameFormat<FrameHeader<0xFD, 0xFC, 0xFB, 0xFA>, PayloadLengthU16LE<LengthField{.offset = 4, .trailer = 4}>>;
using LdData =
    FrameFormat<FrameHeader<0xF4, 0xF3, 0xF2, 0xF1>, PayloadLengthU16LE<LengthField{.offset = 4, .trailer = 4}>>;
// ld2410s short frame: 6E, always 5 bytes
using LdShort = FrameFormat<FrameHeader<0x6E>, FrameFixedLength<5>>;
// Small format for the recovery tests: A5, one byte at 1 holds the whole length
using Small = FrameFormat<FrameHeader<0xA5>, FrameLengthU8<LengthField{.offset = 1}>>;

static_assert(sizeof(LengthFrameReader<255, OneByte>) == 258);
static_assert(sizeof(LengthFrameReader<300, TwoByteBE>) == 306);

TEST(LengthFrameReaderTest, OneByteLength) {
  LengthFrameReader<16, OneByte> reader;
  EXPECT_EQ(feed_all(reader, {0x00, 0xA5, 0x01, 0x10, 0x11, 0x12, 0x13, 0x00, 0x09, 0x42}),
            (Frames{{0xA5, 0x01, 0x10, 0x11, 0x12, 0x13, 0x00, 0x09, 0x42}}));
}

TEST(LengthFrameReaderTest, TwoByteBigEndianPayloadLength) {
  LengthFrameReader<300, TwoByteBE> reader;
  // Payload of 0x0102 = 258 bytes: 265 in all, past an 8-bit index
  Bytes input{0x55, 0xAA, 0x03, 0x07, 0x01, 0x02};
  input.resize(265, 0x33);
  Frames frames = feed_all(reader, input);
  ASSERT_EQ(frames.size(), 1u);
  EXPECT_EQ(frames[0].size(), 265u);
  // Too long for N: not a frame
  EXPECT_EQ(feed_all(reader, {0x55, 0xAA, 0x03, 0x07, 0x01, 0x30}), Frames{});
  EXPECT_EQ(reader.length(), 0u);
}

TEST(LengthFrameReaderTest, LittleEndianLengthsAboveAByte) {
  // Whole-frame length 0x0102 = 258, low byte first
  LengthFrameReader<300, FrameFormat<FrameHeader<0xA5>, FrameLengthU16LE<LengthField{.offset = 1}>>> whole;
  Bytes input{0xA5, 0x02, 0x01};
  input.resize(258, 0x33);
  Frames frames = feed_all(whole, input);
  ASSERT_EQ(frames.size(), 1u);
  EXPECT_EQ(frames[0].size(), 258u);
  // Payload length 0x0100 = 256 after the field, then a 1 byte trailer: 260 in all
  LengthFrameReader<300, FrameFormat<FrameHeader<0xA5>, PayloadLengthU16LE<LengthField{.offset = 1, .trailer = 1}>>>
      payload;
  input = {0xA5, 0x00, 0x01};
  input.resize(260, 0x44);
  frames = feed_all(payload, input);
  ASSERT_EQ(frames.size(), 1u);
  EXPECT_EQ(frames[0].size(), 260u);
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

TEST(LengthFrameReaderTest, BrokenHeaderHoldsAShorterFrameOfAnotherFormat) {
  // FD FC FB then 77 breaks the command header; its replay finds a whole 2 byte FC frame and keeps the 77 behind it
  LengthFrameReader<32, LdCmd, FrameFormat<FrameHeader<0xFC>, FrameFixedLength<2>>> reader;
  EXPECT_EQ(feed_all(reader, {0xFD, 0xFC, 0xFB, 0x77}), (Frames{{0xFC, 0xFB}}));
  EXPECT_EQ(reader.length(), 0u);
  // The held 77 was noise, so the next frame is found normally
  EXPECT_EQ(feed_all(reader, {0xFC, 0x01}), (Frames{{0xFC, 0x01}}));
}

TEST(LengthFrameReaderTest, RejectedFrameHoldsFramesOfBothFormats) {
  LengthFrameReader<32, LdShort, Small> reader;
  // A rejected 12 byte A5 frame holds a whole short frame, a whole A5 frame and the start of another
  auto reject_12 = [](const Bytes &frame) { return frame.size() == 12; };
  EXPECT_EQ(feed_all(reader, {0xA5, 0x0C, 0x6E, 0x01, 0x02, 0x03, 0x62, 0xA5, 0x03, 0x09, 0xA5, 0x05}, reject_12),
            (Frames{{0xA5, 0x0C, 0x6E, 0x01, 0x02, 0x03, 0x62, 0xA5, 0x03, 0x09, 0xA5, 0x05},
                    {0x6E, 0x01, 0x02, 0x03, 0x62},
                    {0xA5, 0x03, 0x09}}));
  EXPECT_EQ(reader.length(), 2u);
  EXPECT_EQ(feed_all(reader, {0x11, 0x22, 0x33}), (Frames{{0xA5, 0x05, 0x11, 0x22, 0x33}}));
}

TEST(LengthFrameReaderTest, BadLengthRealignsAfterFirstByte) {
  LengthFrameReader<8, Small> reader;
  // 0x01 ends the frame before its length byte and 0x09 is longer than 8: neither is a frame.
  // The second A5 inside the bad one starts the real frame.
  EXPECT_EQ(feed_all(reader, {0xA5, 0x01, 0xA5, 0x09, 0xA5, 0x04, 0x01, 0x02}), (Frames{{0xA5, 0x04, 0x01, 0x02}}));
}

TEST(LengthFrameReaderTest, LengthByteCanStartHeader) {
  LengthFrameReader<8, FrameFormat<FrameHeader<0x09>, FrameLengthU8<LengthField{.offset = 1}>>> reader;
  // Length 9 is longer than 8, so the length byte becomes the next header
  EXPECT_EQ(feed_all(reader, {0x09, 0x09, 0x03, 0x07}), (Frames{{0x09, 0x03, 0x07}}));
}

TEST(LengthFrameReaderTest, LengthTracksPartialFrame) {
  LengthFrameReader<8, Small> reader;
  EXPECT_EQ(feed_all(reader, {0x01, 0xA5, 0x05}), Frames{});
  EXPECT_EQ(reader.length(), 2u);
  EXPECT_EQ(feed_all(reader, {0x10, 0x20, 0x30}), (Frames{{0xA5, 0x05, 0x10, 0x20, 0x30}}));
  EXPECT_EQ(reader.length(), 0u);
}

TEST(LengthFrameReaderTest, RejectedFrameIsSearchedForAHeader) {
  LengthFrameReader<8, Small> reader;
  // The A5 taken as a header was payload; the real frame starts at the second A5
  auto reject_long = [](const Bytes &frame) { return frame.size() == 5; };
  EXPECT_EQ(feed_all(reader, {0xA5, 0x05, 0x00, 0xA5, 0x04}, reject_long), (Frames{{0xA5, 0x05, 0x00, 0xA5, 0x04}}));
  EXPECT_EQ(reader.length(), 2u);
  EXPECT_EQ(feed_all(reader, {0x10, 0x20}, reject_long), (Frames{{0xA5, 0x04, 0x10, 0x20}}));
}

TEST(LengthFrameReaderTest, RejectionRecoversEveryFrameInside) {
  LengthFrameReader<16, Small> reader;
  // The 9 byte frame is rejected; the two frames inside it are handed over too, and a partial one is kept
  auto reject_9 = [](const Bytes &frame) { return frame.size() == 9; };
  EXPECT_EQ(feed_all(reader, {0xA5, 0x09, 0xA5, 0x03, 0x01, 0xA5, 0x02, 0xA5, 0x04}, reject_9),
            (Frames{{0xA5, 0x09, 0xA5, 0x03, 0x01, 0xA5, 0x02, 0xA5, 0x04}, {0xA5, 0x03, 0x01}, {0xA5, 0x02}}));
  EXPECT_EQ(reader.length(), 2u);
  EXPECT_EQ(feed_all(reader, {0x01, 0x02}), (Frames{{0xA5, 0x04, 0x01, 0x02}}));
}

TEST(LengthFrameReaderTest, RecoveredFrameRejectedTooIsSearched) {
  LengthFrameReader<16, Small> reader;
  // The 9 byte frame holds a 4 byte candidate, which holds a 3 byte one; both outer ones are rejected
  auto reject_outer = [](const Bytes &frame) { return frame.size() != 3; };
  EXPECT_EQ(
      feed_all(reader, {0xA5, 0x09, 0xA5, 0x04, 0xA5, 0x03, 0x01, 0x02, 0x03}, reject_outer),
      (Frames{{0xA5, 0x09, 0xA5, 0x04, 0xA5, 0x03, 0x01, 0x02, 0x03}, {0xA5, 0x04, 0xA5, 0x03}, {0xA5, 0x03, 0x01}}));
  EXPECT_EQ(reader.length(), 0u);
}

TEST(LengthFrameReaderTest, FlushHandsOverFramesHeldBack) {
  LengthFrameReader<16, Small> reader;
  Collect handle;
  // A corrupt length of 15 holds back two complete frames until flush()
  for (uint8_t c : {0xA5, 0x0F, 0xA5, 0x04, 0x01, 0x02, 0xA5, 0x03, 0x09})
    reader.feed(c, handle);
  EXPECT_EQ(handle.seen, Frames{});
  EXPECT_EQ(reader.length(), 9u);
  reader.flush(handle);
  EXPECT_EQ(handle.seen, (Frames{{0xA5, 0x04, 0x01, 0x02}, {0xA5, 0x03, 0x09}}));
  EXPECT_EQ(reader.length(), 0u);
  reader.flush(handle);  // nothing collected: nothing to do
  EXPECT_EQ(handle.seen.size(), 2u);
}

TEST(LengthFrameReaderTest, FlushKeepsASlowFrameUntilTheRestArrives) {
  LengthFrameReader<16, Small> reader;
  Collect handle;
  for (uint8_t c : {0xA5, 0x06, 0xA5, 0x03})
    reader.feed(c, handle);
  reader.flush(handle);
  EXPECT_EQ(handle.seen, Frames{});
  EXPECT_EQ(reader.length(), 4u);
  for (uint8_t c : {0x01, 0x02})
    reader.feed(c, handle);
  EXPECT_EQ(handle.seen, (Frames{{0xA5, 0x06, 0xA5, 0x03, 0x01, 0x02}}));
}

TEST(LengthFrameReaderTest, FlushKeepsASlowFrameBehindTheFramesItHandsOver) {
  LengthFrameReader<16, Small> reader;
  Collect handle;
  // A corrupt length holds a whole frame, then the start of a slow one
  for (uint8_t c : {0xA5, 0x0F, 0xA5, 0x04, 0x01, 0x02, 0xA5, 0x05, 0x11})
    reader.feed(c, handle);
  reader.flush(handle);
  reader.flush(handle);
  EXPECT_EQ(handle.seen, (Frames{{0xA5, 0x04, 0x01, 0x02}}));
  EXPECT_EQ(reader.length(), 3u);
  for (uint8_t c : {0x22, 0x33})
    reader.feed(c, handle);
  EXPECT_EQ(handle.seen, (Frames{{0xA5, 0x04, 0x01, 0x02}, {0xA5, 0x05, 0x11, 0x22, 0x33}}));
}

TEST(LengthFrameReaderTest, ReceivingUntilAnIdleFlushKeepsTheFrame) {
  LengthFrameReader<16, Small> reader;
  Collect handle;
  EXPECT_FALSE(reader.receiving());
  for (uint8_t c : {0xA5, 0x06, 0x01})
    reader.feed(c, handle);
  EXPECT_TRUE(reader.receiving());
  reader.flush(handle);
  EXPECT_FALSE(reader.receiving());
  reader.feed(0x02, handle);
  EXPECT_TRUE(reader.receiving());
}

TEST(LengthFrameReaderTest, NewBytesAtTheSearchedLengthAreSearchedAgain) {
  // A5 with a whole-frame length at 5, and a fixed two-byte 6E frame
  LengthFrameReader<16, FrameFormat<FrameHeader<0xA5>, FrameLengthU8<LengthField{.offset = 5}>>,
                    FrameFormat<FrameHeader<0x6E>, FrameFixedLength<2>>>
      reader;
  Collect handle;
  for (uint8_t c : {0xA5, 0xA5, 0x00, 0x00, 0x6E})
    reader.feed(c, handle);
  reader.flush(handle);
  reader.feed(0x01, handle);
  reader.flush(handle);
  EXPECT_EQ(handle.seen, (Frames{{0x6E, 0x01}}));
}

TEST(LengthFrameReaderTest, ResyncAfterASearchedFrameIsSearchedAgain) {
  // A5 with a whole-frame length at 3, and a fixed two-byte AA frame
  LengthFrameReader<6, FrameFormat<FrameHeader<0xA5>, FrameLengthU8<LengthField{.offset = 3}>>,
                    FrameFormat<FrameHeader<0xAA>, FrameFixedLength<2>>>
      reader;
  Collect handle;
  for (uint8_t c : {0xA5, 0xA5, 0xAA})
    reader.feed(c, handle);
  reader.flush(handle);
  reader.feed(0x07, handle);
  reader.flush(handle);
  EXPECT_EQ(handle.seen, (Frames{{0xAA, 0x07}}));
}

TEST(LengthFrameReaderTest, FlushKeepsTheFrameWhenEveryFrameInsideIsRejected) {
  LengthFrameReader<16, Small> reader;
  size_t calls = 0;
  auto reject = [&calls](const uint8_t *, size_t) {
    calls++;
    return false;
  };
  for (uint8_t c : {0xA5, 0x0F, 0xA5, 0x03, 0x09})
    reader.feed(c, reject);
  reader.flush(reject);
  EXPECT_EQ(calls, 1u);
  EXPECT_EQ(reader.length(), 5u);
  // Searched once until a new byte comes
  reader.flush(reject);
  EXPECT_EQ(calls, 1u);
}

TEST(LengthFrameReaderTest, ResetForgetsThePartialFrame) {
  LengthFrameReader<16, Small> reader;
  EXPECT_EQ(feed_all(reader, {0xA5, 0x05, 0x11}), Frames{});
  reader.reset();
  EXPECT_EQ(reader.length(), 0u);
  EXPECT_EQ(feed_all(reader, {0xA5, 0x03, 0x22}), (Frames{{0xA5, 0x03, 0x22}}));
}

TEST(LengthFrameReaderTest, DropKeptLeavesAFrameStillComingIn) {
  LengthFrameReader<16, Small> reader;
  Collect handle;
  for (uint8_t c : {0xA5, 0x06, 0x01})
    reader.feed(c, handle);
  reader.drop_kept();
  EXPECT_EQ(reader.length(), 3u);
  reader.flush(handle);
  reader.drop_kept();
  EXPECT_EQ(reader.length(), 0u);
}

TEST(LengthFrameReaderTest, DropKeptInsideAHandlerKeepsTheHeldFrames) {
  LengthFrameReader<16, Small> reader;
  Frames seen;
  auto handle = [&reader, &seen](const uint8_t *frame, size_t len) {
    seen.emplace_back(frame, frame + len);
    reader.drop_kept();
    return frame[1] != 0x09;
  };
  for (uint8_t c : {0xA5, 0x09, 0xA5, 0x03, 0x01, 0xA5, 0x03, 0x02, 0x00})
    reader.feed(c, handle);
  EXPECT_EQ(seen,
            (Frames{{0xA5, 0x09, 0xA5, 0x03, 0x01, 0xA5, 0x03, 0x02, 0x00}, {0xA5, 0x03, 0x01}, {0xA5, 0x03, 0x02}}));
}

TEST(LengthFrameReaderTest, IdleAfterHandledFrameFindsNothingInIt) {
  LengthFrameReader<16, Small> reader;
  Collect handle;
  // A handled frame whose payload looks like a frame of its own, then noise
  for (uint8_t c : {0xA5, 0x06, 0xA5, 0x03, 0x01, 0x02, 0x00})
    reader.feed(c, handle);
  reader.flush(handle);
  EXPECT_EQ(handle.seen, (Frames{{0xA5, 0x06, 0xA5, 0x03, 0x01, 0x02}}));
}

// A custom rule known at the first byte that reads the buffer builds (only FrameFixedLength is checked up front)
struct FirstByteLength {
  static constexpr size_t KNOWN_AT = 1;
  static constexpr size_t get(const uint8_t *buf) { return buf[0]; }
};

TEST(LengthFrameReaderTest, CustomRuleKnownAtTheFirstByte) {
  LengthFrameReader<8, FrameFormat<FrameHeader<0x03>, FirstByteLength>> reader;
  EXPECT_EQ(feed_all(reader, {0x03, 0x01, 0x02, 0x03}), (Frames{{0x03, 0x01, 0x02}}));
}

// A5, payload length at 1, payload, checksum; a frame has at least 2 payload bytes
using MinTwo =
    FrameFormat<FrameHeader<0xA5>, PayloadLengthU8<LengthField{.offset = 1, .trailer = 1}>, FrameMinLength<5>>;

TEST(LengthFrameReaderTest, LengthBelowTheMinimumIsRejectedAtTheHeader) {
  LengthFrameReader<16, MinTwo> reader;
  EXPECT_EQ(feed_all(reader, {0xA5, 0x00, 0xA5, 0x02, 0x10, 0x20, 0x30}), (Frames{{0xA5, 0x02, 0x10, 0x20, 0x30}}));
}

static bool second_byte_not_ff(const uint8_t *buf) { return buf[1] != 0xFF; }
// A5, a byte that must not be FF, payload length at 2, payload
using Checked =
    FrameFormat<FrameHeader<0xA5>, CheckedLength<PayloadLengthU8<LengthField{.offset = 2}>, 3, second_byte_not_ff>>;

static size_t &warned_len() {
  static size_t len = 0;
  return len;
}
static void note_too_long(size_t len) { warned_len() = len; }

TEST(LengthFrameReaderTest, WarnIfLongerRejectsAndReports) {
  LengthFrameReader<
      8, FrameFormat<FrameHeader<0xA5>, WarnIfLonger<FrameLengthU8<LengthField{.offset = 1}>, 8, note_too_long>>>
      reader;
  EXPECT_EQ(feed_all(reader, {0xA5, 0x09, 0xA5, 0x03, 0x42}), (Frames{{0xA5, 0x03, 0x42}}));
  EXPECT_EQ(warned_len(), 9u);
}

TEST(LengthFrameReaderTest, CheckedLengthRejectsABadHeader) {
  LengthFrameReader<16, Checked> reader;
  EXPECT_EQ(feed_all(reader, {0xA5, 0xFF, 0x01, 0xA5, 0x00, 0x01, 0x42}), (Frames{{0xA5, 0x00, 0x01, 0x42}}));
}

}  // namespace esphome::core::testing
