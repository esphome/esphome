#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "esphome/components/ld600x/ld600x_frame.h"

namespace esphome::ld600x::testing {

namespace {

uint8_t checksum(const std::vector<uint8_t> &bytes) {
  return static_cast<uint8_t>(~xor_bytes(bytes.data(), bytes.size()));
}

// Independent of encode_frame, so the parser tests do not rely on the encoder.
std::vector<uint8_t> make_frame(uint16_t id, uint16_t type, const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> frame = {TF_SOF,
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

// SOF, big-endian ID, length, type, header checksum, payload, data checksum.
const std::vector<uint8_t> SAMPLE_PAYLOAD = {0x10, 0x01, 0xFE};
constexpr std::array<uint8_t, 12> SAMPLE_FRAME = {0x01, 0x12, 0x34, 0x00, 0x03, 0x56,
                                                  0x78, 0xF5, 0x10, 0x01, 0xFE, 0x10};

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
    if (complete) {
      completions++;
      expect_frame(parser, id, type, payload);
    }
  }
  EXPECT_EQ(completions, 1u);
}

void feed_valid_frame(FrameParser &parser, uint16_t id, uint16_t type, const std::vector<uint8_t> &payload) {
  feed_valid_frame(parser, make_frame(id, type, payload), id, type, payload);
}

void feed_rejected(FrameParser &parser, const std::vector<uint8_t> &bytes) {
  for (uint8_t byte : bytes) {
    EXPECT_FALSE(parser.feed(byte));
  }
}

const std::vector<uint8_t> NEXT_PAYLOAD = {0x42, 0x81};

}  // namespace

class Ld600xFrameParser : public ::testing::Test {
 protected:
  void SetUp() override { this->parser_.init(this->buffer_.data(), this->buffer_.size()); }

  // Re-initialise on a slice of the buffer with guard bytes filled on both sides.
  void init_guarded(uint16_t capacity) {
    this->guarded_capacity_ = capacity;
    this->buffer_.fill(0xA5);
    this->parser_.init(this->buffer_.data() + 1, capacity);
  }

  void expect_guards_intact() const {
    EXPECT_EQ(this->buffer_.front(), 0xA5);
    EXPECT_EQ(this->buffer_[this->guarded_capacity_ + 1], 0xA5);
  }

  std::array<uint8_t, 16> buffer_{};
  FrameParser parser_;
  uint16_t guarded_capacity_{0};
};

TEST_F(Ld600xFrameParser, CompleteFrameByteByByte) {
  const std::vector<uint8_t> frame = make_frame(0x1234, 0x5678, SAMPLE_PAYLOAD);
  ASSERT_EQ(frame, (std::vector<uint8_t>(SAMPLE_FRAME.begin(), SAMPLE_FRAME.end())));
  feed_valid_frame(this->parser_, frame, 0x1234, 0x5678, SAMPLE_PAYLOAD);
}

TEST_F(Ld600xFrameParser, EmptyPayloadCompletesAtHeaderChecksum) {
  ASSERT_EQ(make_frame(0x1234, 0x5678, {}).size(), 8u);
  feed_valid_frame(this->parser_, 0x1234, 0x5678, {});
  feed_valid_frame(this->parser_, 0x1234, 0x5678, {});
}

TEST_F(Ld600xFrameParser, HeaderNeverTouchesTheBuffer) {
  // A buffer smaller than the header, guarded on both sides.
  this->init_guarded(2);
  const std::array<uint8_t, 16> untouched = this->buffer_;
  feed_rejected(this->parser_, make_frame(0x1234, 0x5678, {0x10, 0x20, 0x30}));
  EXPECT_EQ(this->buffer_, untouched);
  feed_valid_frame(this->parser_, 0x1234, 0x5678, NEXT_PAYLOAD);
  this->expect_guards_intact();
}

TEST(Ld600xFrameParserUninitialised, FeedBeforeInitSkipsPayloads) {
  FrameParser parser;
  feed_rejected(parser, make_frame(0x1234, 0x5678, {0x01, 0x02}));
  feed_valid_frame(parser, 0x1234, 0x5678, {});
}

enum class Damage : uint8_t { HEADER_CHECKSUM, DATA_CHECKSUM };

class Ld600xFrameDamage : public Ld600xFrameParser, public ::testing::WithParamInterface<Damage> {
 protected:
  // An empty frame ends with its header checksum, a non-empty one with its data checksum.
  std::vector<uint8_t> damaged_frame() const {
    return make_frame(0x1234, 0x5678,
                      GetParam() == Damage::HEADER_CHECKSUM ? std::vector<uint8_t>{} : std::vector<uint8_t>{1, 2, 3});
  }
};

TEST_P(Ld600xFrameDamage, ChecksumMismatchDropsFrameAndResynchronises) {
  std::vector<uint8_t> corrupt = this->damaged_frame();
  corrupt.back() ^= 0x80;
  feed_rejected(this->parser_, corrupt);
  feed_valid_frame(this->parser_, 0xABCD, 0x9876, NEXT_PAYLOAD);
}

TEST_P(Ld600xFrameDamage, DroppedChecksumKeepsNextFrame) {
  std::vector<uint8_t> truncated = this->damaged_frame();
  truncated.pop_back();  // The checksum byte never arrives.
  const std::vector<uint8_t> next = make_frame(0xABCD, 0x9876, NEXT_PAYLOAD);
  feed_rejected(this->parser_, truncated);
  // The next SOF lands where the checksum was expected: it fails the check, then starts the next header.
  EXPECT_FALSE(this->parser_.feed(next[0]));
  feed_valid_frame(this->parser_, {next.begin() + 1, next.end()}, 0xABCD, 0x9876, NEXT_PAYLOAD);
}

INSTANTIATE_TEST_SUITE_P(Checksums, Ld600xFrameDamage,
                         ::testing::Values(Damage::HEADER_CHECKSUM, Damage::DATA_CHECKSUM));

TEST_F(Ld600xFrameParser, OversizedPayloadAndChecksumAreDiscardedWithoutBufferWrites) {
  // A complete frame inside the discarded payload must not be parsed.
  std::vector<uint8_t> payload = make_frame(0x1234, 0x5678, {});
  payload.push_back(checksum(payload) ^ 0x01);
  const std::vector<uint8_t> oversized = make_frame(0xABCD, 0x9876, payload);
  ASSERT_EQ(oversized.back(), 0x01);  // The discarded checksum must not start a frame either.
  this->init_guarded(6);
  feed_rejected(this->parser_, {oversized.begin(), oversized.begin() + 8});
  this->expect_guards_intact();
  const std::array<uint8_t, 16> after_header = this->buffer_;
  for (size_t i = 8; i < oversized.size(); i++) {
    EXPECT_FALSE(this->parser_.feed(oversized[i]));
    EXPECT_EQ(this->buffer_, after_header);
  }
  feed_valid_frame(this->parser_, 0x1234, 0x5678, NEXT_PAYLOAD);
  this->expect_guards_intact();
}

TEST_F(Ld600xFrameParser, MaximumLengthOversizedFrameIsSkippedExactly) {
  EXPECT_EQ(this->parser_.capacity(), this->buffer_.size());
  std::vector<uint8_t> header = {TF_SOF, 0x12, 0x34, 0xFF, 0xFF, 0x56, 0x78};
  header.push_back(checksum(header));
  feed_rejected(this->parser_, header);
  // Payload and checksum made of SOF bytes: any miscount would start a header from one of them.
  for (uint32_t i = 0; i < 0x10000; i++) {
    ASSERT_FALSE(this->parser_.feed(TF_SOF)) << "byte " << i;
  }
  feed_valid_frame(this->parser_, 0xABCD, 0x9876, NEXT_PAYLOAD);
}

TEST_F(Ld600xFrameParser, GarbageBetweenFramesIsIgnored) {
  feed_valid_frame(this->parser_, 0x1234, 0x5678, {0x42});
  feed_rejected(this->parser_, {0x00, 0xFF, 0xA5, 0x5A, 0x02, 0x80});
  feed_valid_frame(this->parser_, 0xABCD, 0x9876, {0x81});
}

TEST(Ld600xEncodeFrame, PayloadLayoutAndRoundTrip) {
  std::array<uint8_t, 12> encoded{};
  ASSERT_EQ(encode_frame(0x1234, 0x5678, SAMPLE_PAYLOAD.data(), SAMPLE_PAYLOAD.size(), encoded.data()),
            SAMPLE_FRAME.size());
  EXPECT_EQ(encoded, SAMPLE_FRAME);
  std::array<uint8_t, 16> buffer{};
  FrameParser parser;
  parser.init(buffer.data(), buffer.size());
  feed_valid_frame(parser, {encoded.begin(), encoded.end()}, 0x1234, 0x5678, SAMPLE_PAYLOAD);
}

TEST(Ld600xEncodeFrame, EmptyPayloadLayoutAndRoundTrip) {
  // The trailing guard byte must not be written.
  const std::array<uint8_t, 9> expected = {0x01, 0x12, 0x34, 0x00, 0x00, 0x56, 0x78, 0xF6, 0xA5};
  std::array<uint8_t, 9> encoded{};
  encoded.fill(0xA5);
  ASSERT_EQ(encode_frame(0x1234, 0x5678, nullptr, 0, encoded.data()), 8u);
  EXPECT_EQ(encoded, expected);
  std::array<uint8_t, 6> buffer{};
  FrameParser parser;
  parser.init(buffer.data(), buffer.size());
  feed_valid_frame(parser, {encoded.begin(), encoded.begin() + 8}, 0x1234, 0x5678, {});
}

TEST(Ld600xEncodeFrame, SixteenBitLengthMatchesReference) {
  const std::vector<uint8_t> payload(300, 0x5A);
  std::vector<uint8_t> encoded(TF_FRAME_OVERHEAD + payload.size());
  ASSERT_EQ(encode_frame(0x8001, 0x0A08, payload.data(), payload.size(), encoded.data()), encoded.size());
  EXPECT_EQ(encoded, make_frame(0x8001, 0x0A08, payload));
}

TEST(Ld600xFrameId, SetsTopBitAndWrapsLowBits) {
  uint16_t counter = 0;
  EXPECT_EQ(next_frame_id(counter), 0x8000);
  EXPECT_EQ(next_frame_id(counter), 0x8001);
  counter = 0x7FFF;
  EXPECT_EQ(next_frame_id(counter), 0xFFFF);
  EXPECT_EQ(next_frame_id(counter), 0x8000);
  counter = 0xFFFF;
  EXPECT_EQ(next_frame_id(counter), 0xFFFF);
  EXPECT_EQ(counter, 0);
}

TEST(Ld600xByteHelpers, UnsignedInt32) {
  const std::array<uint8_t, 4> bytes = {0x12, 0x34, 0x56, 0xF8};
  std::array<uint8_t, 4> written{};
  write_u32_le(written.data(), 0xF8563412u);
  EXPECT_EQ(written, bytes);
  write_u32_le(written.data(), static_cast<uint32_t>(-2));
  EXPECT_EQ(written, (std::array<uint8_t, 4>{0xFE, 0xFF, 0xFF, 0xFF}));
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
