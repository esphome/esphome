#include <cstdint>
#include <cstring>
#include <vector>
#include <gtest/gtest.h>
#include "esphome/components/ld24xx/ld24xx.h"

namespace esphome::ld24xx::testing {

namespace {
constexpr uint8_t DATA_FRAME_HEADER[HEADER_FOOTER_SIZE] = {0xF4, 0xF3, 0xF2, 0xF1};
constexpr uint8_t DATA_FRAME_FOOTER[HEADER_FOOTER_SIZE] = {0xF8, 0xF7, 0xF6, 0xF5};
constexpr uint8_t DATA_FRAME_SIZE = 12;  // header, four payload bytes, footer
constexpr size_t BUFFER_SIZE = 40;

// Command frame with the given payload bytes after the count.
std::vector<uint8_t> make_cmd_frame(const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> frame(std::begin(CMD_FRAME_HEADER), std::end(CMD_FRAME_HEADER));
  frame.push_back(payload.size());
  frame.push_back(0x00);
  frame.insert(frame.end(), payload.begin(), payload.end());
  frame.insert(frame.end(), std::begin(CMD_FRAME_FOOTER), std::end(CMD_FRAME_FOOTER));
  return frame;
}

std::vector<uint8_t> make_data_frame(uint8_t fill) {
  std::vector<uint8_t> frame(std::begin(DATA_FRAME_HEADER), std::end(DATA_FRAME_HEADER));
  frame.insert(frame.end(), DATA_FRAME_SIZE - 2 * HEADER_FOOTER_SIZE, fill);
  frame.insert(frame.end(), std::begin(DATA_FRAME_FOOTER), std::end(DATA_FRAME_FOOTER));
  return frame;
}
}  // namespace

// A parser like the radar components': data frames are fixed size, command frames carry their count.
class LD24xxFrameSplitterTest : public ::testing::Test {
 protected:
  void feed_(const std::vector<uint8_t> &bytes) {
    for (uint8_t byte : bytes) {
      feed_frame_byte(
          this->buffer_, this->pos_, byte, DATA_FRAME_HEADER,
          [](const uint8_t *buffer, uint8_t count) -> uint32_t {
            if (count < HEADER_FOOTER_SIZE) {
              return 0;
            }
            return buffer[0] == DATA_FRAME_HEADER[0] ? DATA_FRAME_SIZE : cmd_frame_length(buffer, count);
          },
          [this](uint8_t frame_len) { return this->handle_frame_(frame_len); }, [this]() { this->drops_++; });
    }
  }

  bool handle_frame_(uint8_t frame_len) {
    const uint8_t *footer = this->buffer_[0] == DATA_FRAME_HEADER[0] ? DATA_FRAME_FOOTER : CMD_FRAME_FOOTER;
    if (!validate_header_footer(footer, &this->buffer_[frame_len - HEADER_FOOTER_SIZE])) {
      return false;
    }
    this->frames_.emplace_back(this->buffer_, this->buffer_ + frame_len);
    return true;
  }

  uint8_t buffer_[BUFFER_SIZE];
  uint8_t pos_{0};
  std::vector<std::vector<uint8_t>> frames_;
  int drops_{0};
};

TEST_F(LD24xxFrameSplitterTest, FooterBytesInThePayloadDoNotEndTheFrame) {
  auto frame = make_cmd_frame({0x61, 0x01, 0x00, 0x00, 0x04, 0x03, 0x02, 0x01, 0x55});
  this->feed_(frame);
  ASSERT_EQ(this->frames_.size(), 1u);
  EXPECT_EQ(this->frames_[0], frame);
  EXPECT_EQ(this->pos_, 0);
  EXPECT_EQ(this->drops_, 0);
}

TEST_F(LD24xxFrameSplitterTest, BytesBeforeAHeaderAreNeverBuffered) {
  this->feed_({0x00, 0x11, 0xFC, 0xFB, 0xFA, 0x04});
  EXPECT_EQ(this->pos_, 0);
  this->feed_(make_data_frame(0x22));
  ASSERT_EQ(this->frames_.size(), 1u);
  EXPECT_EQ(this->frames_[0], make_data_frame(0x22));
}

TEST_F(LD24xxFrameSplitterTest, AHeaderRestartsWhenItsBytesStopMatching) {
  // The first byte picks the command header, so F3 cannot continue it, and FD after FC starts over
  this->feed_({0xFD, 0xF3, 0xFD, 0xFC, 0xFD, 0xFC, 0xFB, 0xFA, 0x00, 0x00});
  EXPECT_EQ(this->pos_, 6) << "only the header that matched through and its count are buffered";
  this->feed_({0x04, 0x03, 0x02, 0x01});
  ASSERT_EQ(this->frames_.size(), 1u);
  EXPECT_EQ(this->frames_[0], make_cmd_frame({}));
  EXPECT_EQ(this->drops_, 0);
}

TEST_F(LD24xxFrameSplitterTest, ImpossibleCountsAreDroppedAndTheNextFrameParsed) {
  // 0xFFF6 totals 65536, which must not wrap to 0; 0xFFFF is simply too long for the buffer
  this->feed_({0xFD, 0xFC, 0xFB, 0xFA, 0xF6, 0xFF});
  this->feed_({0xFD, 0xFC, 0xFB, 0xFA, 0xFF, 0xFF});
  EXPECT_EQ(this->drops_, 2);
  EXPECT_EQ(this->pos_, 0);
  this->feed_(make_data_frame(0x33));
  ASSERT_EQ(this->frames_.size(), 1u);
  EXPECT_EQ(this->frames_[0], make_data_frame(0x33));
}

TEST_F(LD24xxFrameSplitterTest, AFrameCaughtInsideADroppedFrameIsReplayed) {
  // A header whose count reaches 20 bytes into the stream swallows the whole frame that follows
  std::vector<uint8_t> bytes = {0xFD, 0xFC, 0xFB, 0xFA, 0x0A, 0x00, 0x61, 0x01};
  auto inner = make_data_frame(0x44);
  bytes.insert(bytes.end(), inner.begin(), inner.end());
  this->feed_(bytes);
  EXPECT_EQ(this->drops_, 1);
  ASSERT_EQ(this->frames_.size(), 1u);
  EXPECT_EQ(this->frames_[0], inner);
  EXPECT_EQ(this->pos_, 0);
}

TEST_F(LD24xxFrameSplitterTest, EveryWholeFrameLeftByADropIsDrained) {
  // The dropped frame's window holds two whole data frames and the start of a third
  std::vector<uint8_t> bytes = {0xFD, 0xFC, 0xFB, 0xFA, 0x18, 0x00, 0x61, 0x01};
  for (uint8_t fill : {0x55, 0x66}) {
    auto inner = make_data_frame(fill);
    bytes.insert(bytes.end(), inner.begin(), inner.end());
  }
  bytes.insert(bytes.end(), {0xF4, 0xF3});
  this->feed_(bytes);
  EXPECT_EQ(this->drops_, 1);
  ASSERT_EQ(this->frames_.size(), 2u);
  EXPECT_EQ(this->frames_[0], make_data_frame(0x55));
  EXPECT_EQ(this->frames_[1], make_data_frame(0x66));
  EXPECT_EQ(this->pos_, 2) << "the partial header at the end stays buffered";
}

TEST_F(LD24xxFrameSplitterTest, AWrongFooterDropsOnlyThatFrame) {
  auto bad = make_cmd_frame({0x01, 0x02});
  bad[bad.size() - 1] = 0xEE;
  this->feed_(bad);
  EXPECT_EQ(this->drops_, 1);
  EXPECT_TRUE(this->frames_.empty());
  this->feed_(make_cmd_frame({0x03}));
  ASSERT_EQ(this->frames_.size(), 1u);
  EXPECT_EQ(this->frames_[0], make_cmd_frame({0x03}));
}

TEST_F(LD24xxFrameSplitterTest, BackToBackFramesOfBothKinds) {
  this->feed_(make_data_frame(0x01));
  this->feed_(make_cmd_frame({0xA0, 0x01, 0x00, 0x00}));
  this->feed_(make_data_frame(0x02));
  ASSERT_EQ(this->frames_.size(), 3u);
  EXPECT_EQ(this->frames_[1], make_cmd_frame({0xA0, 0x01, 0x00, 0x00}));
  EXPECT_EQ(this->frames_[2], make_data_frame(0x02));
  EXPECT_EQ(this->drops_, 0);
}

}  // namespace esphome::ld24xx::testing
