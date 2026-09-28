#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "esphome/components/ld600x/ld600x_frame.h"

namespace esphome::ld600x::testing {

namespace {

uint8_t checksum(const std::vector<uint8_t> &bytes) {
  uint8_t result = 0xFF;
  for (uint8_t byte : bytes) {
    result ^= byte;
  }
  return result;
}

std::vector<uint8_t> make_frame(uint16_t id, uint16_t type, const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> frame = {0x01,
                                static_cast<uint8_t>(id >> 8),
                                static_cast<uint8_t>(id),
                                static_cast<uint8_t>(payload.size() >> 8),
                                static_cast<uint8_t>(payload.size()),
                                static_cast<uint8_t>(type >> 8),
                                static_cast<uint8_t>(type)};
  frame.push_back(checksum(frame));
  frame.insert(frame.end(), payload.begin(), payload.end());
  if (!payload.empty()) {
    frame.push_back(checksum(payload));
  }
  return frame;
}

void expect_frame(const FrameParser &parser, uint16_t id, uint16_t type, const std::vector<uint8_t> &payload) {
  EXPECT_EQ(parser.id(), id);
  EXPECT_EQ(parser.type(), type);
  ASSERT_EQ(parser.length(), payload.size());
  if (payload.empty()) {
    EXPECT_EQ(parser.data(), nullptr);
  } else {
    ASSERT_NE(parser.data(), nullptr);
    EXPECT_EQ(std::vector<uint8_t>(parser.data(), parser.data() + parser.length()), payload);
  }
}

void feed_valid_frame(FrameParser &parser, const std::vector<uint8_t> &frame, uint16_t id, uint16_t type,
                      const std::vector<uint8_t> &payload) {
  size_t completions = 0;
  for (size_t i = 0; i < frame.size(); i++) {
    const bool complete = parser.feed(frame[i]);
    EXPECT_EQ(complete, i == frame.size() - 1) << "byte " << i;
    EXPECT_EQ(parser.event(), FrameEvent::FRAME_EVENT_NONE);
    if (complete) {
      completions++;
      expect_frame(parser, id, type, payload);
    }
  }
  EXPECT_EQ(completions, 1u);
}

}  // namespace

TEST(Ld600xFrameParser, CompleteFrameByteByByte) {
  const std::vector<uint8_t> payload = {0x10, 0x01, 0xFE};
  // SOF, big-endian ID, length, type, header checksum, payload, data checksum.
  const std::array<uint8_t, 12> expected = {0x01, 0x12, 0x34, 0x00, 0x03, 0x56, 0x78, 0xF5, 0x10, 0x01, 0xFE, 0x10};
  const std::vector<uint8_t> frame = make_frame(0x1234, 0x5678, payload);
  ASSERT_EQ(frame, (std::vector<uint8_t>(expected.begin(), expected.end())));
  std::array<uint8_t, 16> buffer{};
  FrameParser parser;
  parser.init(buffer.data(), buffer.size());
  feed_valid_frame(parser, frame, 0x1234, 0x5678, payload);
}

TEST(Ld600xFrameParser, EmptyPayloadCompletesAtHeaderChecksum) {
  const std::vector<uint8_t> frame = make_frame(0x1234, 0x5678, {});
  ASSERT_EQ(frame.size(), 8u);
  std::array<uint8_t, 6> buffer{};
  FrameParser parser;
  parser.init(buffer.data(), buffer.size());
  feed_valid_frame(parser, frame, 0x1234, 0x5678, {});
  feed_valid_frame(parser, frame, 0x1234, 0x5678, {});
}

TEST(Ld600xFrameParser, EverySplitPosition) {
  const std::vector<uint8_t> payload = {0x00, 0x01, 0x80, 0xFF};
  const std::vector<uint8_t> frame = make_frame(0x1234, 0x5678, payload);
  for (size_t split = 0; split <= frame.size(); split++) {
    SCOPED_TRACE(split);
    std::array<uint8_t, 16> buffer{};
    FrameParser parser;
    parser.init(buffer.data(), buffer.size());
    size_t completions = 0;
    for (size_t i = 0; i < split; i++) {
      if (parser.feed(frame[i])) {
        completions++;
        expect_frame(parser, 0x1234, 0x5678, payload);
      }
      EXPECT_EQ(parser.event(), FrameEvent::FRAME_EVENT_NONE);
    }
    for (size_t i = split; i < frame.size(); i++) {
      if (parser.feed(frame[i])) {
        completions++;
        expect_frame(parser, 0x1234, 0x5678, payload);
      }
      EXPECT_EQ(parser.event(), FrameEvent::FRAME_EVENT_NONE);
    }
    EXPECT_EQ(completions, 1u);
  }
}

TEST(Ld600xFrameParser, HeaderChecksumMismatchResynchronises) {
  std::vector<uint8_t> corrupt = make_frame(0x1234, 0x5678, {});
  corrupt[7] ^= 0x80;
  std::array<uint8_t, 16> buffer{};
  FrameParser parser;
  parser.init(buffer.data(), buffer.size());
  for (size_t i = 0; i < corrupt.size(); i++) {
    EXPECT_FALSE(parser.feed(corrupt[i]));
    EXPECT_EQ(parser.event(), i == 7 ? FrameEvent::FRAME_EVENT_HEADER_CHECKSUM_MISMATCH : FrameEvent::FRAME_EVENT_NONE);
  }
  const std::vector<uint8_t> payload = {0x42, 0x81};
  feed_valid_frame(parser, make_frame(0xABCD, 0x9876, payload), 0xABCD, 0x9876, payload);
}

TEST(Ld600xFrameParser, DataChecksumMismatchDropsFrame) {
  std::vector<uint8_t> corrupt = make_frame(0x1234, 0x5678, {0x01, 0x02, 0x03});
  corrupt.back() ^= 0x80;
  std::array<uint8_t, 16> buffer{};
  FrameParser parser;
  parser.init(buffer.data(), buffer.size());
  for (size_t i = 0; i < corrupt.size(); i++) {
    EXPECT_FALSE(parser.feed(corrupt[i]));
    EXPECT_EQ(parser.event(),
              i == corrupt.size() - 1 ? FrameEvent::FRAME_EVENT_DATA_CHECKSUM_MISMATCH : FrameEvent::FRAME_EVENT_NONE);
  }
  const std::vector<uint8_t> payload = {0x42, 0x81};
  feed_valid_frame(parser, make_frame(0xABCD, 0x9876, payload), 0xABCD, 0x9876, payload);
}

TEST(Ld600xFrameParser, OversizedPayloadAndChecksumAreDiscardedWithoutBufferWrites) {
  // A complete frame inside the discarded payload must not be parsed.
  std::vector<uint8_t> payload = make_frame(0x1234, 0x5678, {});
  payload.push_back(checksum(payload) ^ 0x01);
  const std::vector<uint8_t> oversized = make_frame(0xABCD, 0x9876, payload);
  ASSERT_EQ(oversized.back(), 0x01);  // The discarded checksum must not start a frame either.
  std::array<uint8_t, 8> buffer{};
  buffer.fill(0xA5);
  FrameParser parser;
  parser.init(buffer.data() + 1, 6);
  for (size_t i = 0; i < 8; i++) {
    EXPECT_FALSE(parser.feed(oversized[i]));
    EXPECT_EQ(parser.event(), i == 7 ? FrameEvent::FRAME_EVENT_OVERSIZED : FrameEvent::FRAME_EVENT_NONE);
  }
  EXPECT_EQ(buffer.front(), 0xA5);
  EXPECT_EQ(buffer.back(), 0xA5);
  const std::array<uint8_t, 8> after_header = buffer;
  for (size_t i = 8; i < oversized.size(); i++) {
    EXPECT_FALSE(parser.feed(oversized[i]));
    EXPECT_EQ(parser.event(), FrameEvent::FRAME_EVENT_NONE);
    EXPECT_EQ(buffer, after_header);
  }
  const std::vector<uint8_t> valid_payload = {0x42, 0x81};
  feed_valid_frame(parser, make_frame(0x1234, 0x5678, valid_payload), 0x1234, 0x5678, valid_payload);
  EXPECT_EQ(buffer.front(), 0xA5);
  EXPECT_EQ(buffer.back(), 0xA5);
}

TEST(Ld600xFrameParser, GarbageBetweenFramesIsIgnored) {
  std::array<uint8_t, 16> buffer{};
  FrameParser parser;
  parser.init(buffer.data(), buffer.size());
  feed_valid_frame(parser, make_frame(0x1234, 0x5678, {0x42}), 0x1234, 0x5678, {0x42});
  for (uint8_t byte : {0x00, 0xFF, 0xA5, 0x5A, 0x02, 0x80}) {
    EXPECT_FALSE(parser.feed(byte));
    EXPECT_EQ(parser.event(), FrameEvent::FRAME_EVENT_NONE);
  }
  feed_valid_frame(parser, make_frame(0xABCD, 0x9876, {0x81}), 0xABCD, 0x9876, {0x81});
}

TEST(Ld600xEncodeFrame, PayloadLayoutAndRoundTrip) {
  const std::vector<uint8_t> payload = {0x10, 0x01, 0xFE};
  const std::array<uint8_t, 12> expected = {0x01, 0x12, 0x34, 0x00, 0x03, 0x56, 0x78, 0xF5, 0x10, 0x01, 0xFE, 0x10};
  std::array<uint8_t, 12> encoded{};
  ASSERT_EQ(encode_frame(0x1234, 0x5678, payload.data(), payload.size(), encoded.data()), expected.size());
  EXPECT_EQ(encoded, expected);
  std::array<uint8_t, 16> buffer{};
  FrameParser parser;
  parser.init(buffer.data(), buffer.size());
  feed_valid_frame(parser, {encoded.begin(), encoded.end()}, 0x1234, 0x5678, payload);
}

TEST(Ld600xEncodeFrame, EmptyPayloadLayoutAndRoundTrip) {
  const std::array<uint8_t, 8> expected = {0x01, 0x12, 0x34, 0x00, 0x00, 0x56, 0x78, 0xF6};
  std::array<uint8_t, 9> encoded{};
  encoded.fill(0xA5);
  ASSERT_EQ(encode_frame(0x1234, 0x5678, nullptr, 0, encoded.data()), expected.size());
  EXPECT_EQ((std::vector<uint8_t>(encoded.begin(), encoded.begin() + 8)),
            (std::vector<uint8_t>(expected.begin(), expected.end())));
  EXPECT_EQ(encoded[8], 0xA5);
  std::array<uint8_t, 6> buffer{};
  FrameParser parser;
  parser.init(buffer.data(), buffer.size());
  feed_valid_frame(parser, {encoded.begin(), encoded.begin() + 8}, 0x1234, 0x5678, {});
}

TEST(Ld600xByteHelpers, UnsignedIntegers) {
  const std::array<uint8_t, 4> bytes = {0x12, 0x34, 0x56, 0xF8};
  EXPECT_EQ(read_u16_be(bytes.data()), 0x1234);
  EXPECT_EQ(read_u32_le(bytes.data()), 0xF8563412u);
  std::array<uint8_t, 4> written{};
  write_u32_le(written.data(), 0xF8563412u);
  EXPECT_EQ(written, bytes);
  EXPECT_EQ(read_u32_le(written.data()), 0xF8563412u);
}

TEST(Ld600xByteHelpers, NegativeInt32) {
  const std::array<uint8_t, 4> bytes = {0xFE, 0xFF, 0xFF, 0xFF};
  EXPECT_EQ(read_int32_le(bytes.data()), -2);
  std::array<uint8_t, 4> written{};
  write_int32_le(written.data(), -2);
  EXPECT_EQ(written, bytes);
  EXPECT_EQ(read_int32_le(written.data()), -2);
}

TEST(Ld600xByteHelpers, FloatLayoutAndRoundTrip) {
  const std::array<uint8_t, 4> bytes = {0x00, 0x00, 0x48, 0xC1};
  EXPECT_FLOAT_EQ(read_f32_le(bytes.data()), -12.5f);
  std::array<uint8_t, 4> written{};
  write_f32_le(written.data(), -12.5f);
  EXPECT_EQ(written, bytes);
  EXPECT_FLOAT_EQ(read_f32_le(written.data()), -12.5f);
}

}  // namespace esphome::ld600x::testing
