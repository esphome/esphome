#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "esphome/components/ezsp_proxy_tap/ash_acknowledger.h"
#include "esphome/components/ezsp_proxy_tap/ash_protocol.h"
#include "esphome/core/helpers.h"

namespace esphome::ezsp_proxy_tap::testing {

namespace {

// Whole frames as they appear on the wire, each ending in its FLAG. RSTACK and ACK(1)
// are the examples in Silicon Labs UG101.
const std::vector<uint8_t> RSTACK = {0xC1, 0x02, 0x02, 0x9B, 0x7B, 0x7E};
const std::vector<uint8_t> DATA_FRAME_0 = {0x00, 0x43, 0x21, 0xA8, 0x50, 0x9B, 0x98, 0x7E};
const std::vector<uint8_t> DATA_FRAME_1 = {0x10, 0x43, 0x21, 0xA8, 0x50, 0x9F, 0xC2, 0x7E};
const std::vector<uint8_t> DATA_FRAME_0_RETRANSMITTED = {0x08, 0x43, 0x21, 0xA8, 0x50, 0x99, 0xB5, 0x7E};
const std::vector<uint8_t> DATA_FRAME_5 = {0x50, 0x43, 0x21, 0xA8, 0x50, 0x8E, 0xAA, 0x7E};
const std::vector<uint8_t> DATA_FRAME_5_RETRANSMITTED = {0x58, 0x43, 0x21, 0xA8, 0x50, 0x8C, 0x87, 0x7E};
const std::vector<uint8_t> DATA_FRAME_6 = {0x60, 0x43, 0x21, 0xA8, 0x50, 0x82, 0x44, 0x7E};
// Data frame 0 with a 2-byte and a 3-byte data field, below and at the minimum
const std::vector<uint8_t> SHORT_DATA_FRAME_0 = {0x00, 0x43, 0x21, 0xA0, 0x40, 0x7E};
const std::vector<uint8_t> MIN_DATA_FRAME_0 = {0x00, 0x43, 0x21, 0xA8, 0xC1, 0x08, 0x7E};
// Body {0x00, 0x7E, 0x7D, 0x11, 0x13, 0x18, 0x1A}: every reserved byte, each escaped
const std::vector<uint8_t> ESCAPED_FRAME = {0x00, 0x7D, 0x5E, 0x7D, 0x5D, 0x7D, 0x31, 0x7D,
                                            0x33, 0x7D, 0x38, 0x7D, 0x3A, 0x46, 0x8C, 0x7E};

const std::vector<uint8_t> RESERVED_BYTES = {ASH_FLAG_BYTE, ASH_ESCAPE_BYTE,     ASH_XON_BYTE,
                                             ASH_XOFF_BYTE, ASH_SUBSTITUTE_BYTE, ASH_CANCEL_BYTE};

// Data frame 0 with `data_size` bytes of 0x42 and a valid CRC, none of which needs escaping
std::vector<uint8_t> unstuffed_frame(size_t data_size) {
  std::vector<uint8_t> frame(1 + data_size, 0x42);
  frame[0] = 0x00;
  const uint16_t crc = crc16be(frame.data(), frame.size(), ASH_CRC_INIT);
  frame.push_back(crc >> 8);
  frame.push_back(crc & 0xFF);
  frame.push_back(ASH_FLAG_BYTE);
  return frame;
}

std::vector<uint8_t> concat(std::vector<uint8_t> a, const std::vector<uint8_t> &b) {
  a.insert(a.end(), b.begin(), b.end());
  return a;
}

// Returns whether the last byte completed a valid frame
bool feed(AshFrameScanner &scanner, const std::vector<uint8_t> &bytes) {
  bool complete = false;
  for (uint8_t byte : bytes)
    complete = scanner.feed(byte);
  return complete;
}

// Feeds whole frames and returns the ACK owed after the last, or -1 if none is
int feed(AshAcknowledger &acknowledger, const std::vector<uint8_t> &bytes) {
  bool owed = false;
  for (uint8_t byte : bytes)
    owed = acknowledger.feed(byte);
  return owed ? acknowledger.ack_num() : -1;
}

}  // namespace

TEST(AshProtocol, AckFrameMatchesReference) {
  uint8_t frame[ASH_ACK_FRAME_SIZE];
  ash_build_ack_frame(frame, 1);
  EXPECT_EQ(std::vector<uint8_t>(frame, frame + sizeof(frame)), (std::vector<uint8_t>{0x7E, 0x81, 0x60, 0x59, 0x7E}));
}

TEST(AshProtocol, AckFramesNeedNoEscaping) {
  for (uint8_t ack_num = 0; ack_num <= ASH_MAX_SEQUENCE; ack_num++) {
    uint8_t frame[ASH_ACK_FRAME_SIZE];
    ash_build_ack_frame(frame, ack_num);
    EXPECT_EQ(frame[0], ASH_FLAG_BYTE);
    EXPECT_EQ(frame[4], ASH_FLAG_BYTE);
    for (size_t i = 1; i < 4; i++) {
      EXPECT_EQ(std::count(RESERVED_BYTES.begin(), RESERVED_BYTES.end(), frame[i]), 0)
          << "ack_num " << int(ack_num) << " byte " << i;
    }
  }
}

TEST(AshFrameScanner, UnescapesReservedBytes) {
  AshFrameScanner scanner;
  // The CRC covers the unescaped body, so a valid frame proves every byte was restored
  ASSERT_TRUE(feed(scanner, ESCAPED_FRAME));
  EXPECT_EQ(scanner.length(), 7u);
  EXPECT_EQ(std::vector<uint8_t>(scanner.header(), scanner.header() + ASH_HEADER_SIZE),
            (std::vector<uint8_t>{0x00, 0x7E}));
}

TEST(AshFrameScanner, RejectsBadCrc) {
  AshFrameScanner scanner;
  std::vector<uint8_t> corrupt = DATA_FRAME_0;
  corrupt[corrupt.size() - 2] ^= 0x01;
  EXPECT_FALSE(feed(scanner, corrupt));
  EXPECT_TRUE(feed(scanner, DATA_FRAME_0));
}

TEST(AshFrameScanner, CancelDiscardsPartialFrame) {
  AshFrameScanner scanner;
  EXPECT_TRUE(feed(scanner, concat({0x10, 0x43, ASH_CANCEL_BYTE}, DATA_FRAME_0)));
  EXPECT_EQ(scanner.length(), 5u);
}

TEST(AshFrameScanner, SubstituteDiscardsUntilFlag) {
  AshFrameScanner scanner;
  std::vector<uint8_t> substituted = DATA_FRAME_0;
  substituted.insert(substituted.begin() + 2, ASH_SUBSTITUTE_BYTE);
  EXPECT_FALSE(feed(scanner, substituted));
  EXPECT_TRUE(feed(scanner, DATA_FRAME_0));
}

TEST(AshFrameScanner, IgnoresFlowControlInsideFrame) {
  AshFrameScanner scanner;
  std::vector<uint8_t> interrupted = DATA_FRAME_0;
  interrupted.insert(interrupted.begin() + 3, ASH_XOFF_BYTE);
  interrupted.insert(interrupted.begin() + 1, ASH_XON_BYTE);
  ASSERT_TRUE(feed(scanner, interrupted));
  EXPECT_EQ(scanner.length(), 5u);
  EXPECT_EQ(std::vector<uint8_t>(scanner.header(), scanner.header() + ASH_HEADER_SIZE),
            (std::vector<uint8_t>{0x00, 0x43}));
}

TEST(AshFrameScanner, RejectsOversizedFrame) {
  AshFrameScanner scanner;
  EXPECT_TRUE(feed(scanner, unstuffed_frame(ASH_MAX_DATA_FIELD_SIZE)));
  EXPECT_EQ(scanner.length(), 1 + ASH_MAX_DATA_FIELD_SIZE);
  EXPECT_FALSE(feed(scanner, unstuffed_frame(ASH_MAX_DATA_FIELD_SIZE + 1)));
  EXPECT_TRUE(feed(scanner, DATA_FRAME_0));
}

TEST(AshAcknowledger, AcknowledgesFramesInSequence) {
  AshAcknowledger acknowledger;
  EXPECT_EQ(feed(acknowledger, DATA_FRAME_0), 1);
  // A repeat not marked as a retransmission is out of sequence
  EXPECT_EQ(feed(acknowledger, DATA_FRAME_0), -1);
  // A retransmission means the NCP missed our ACK, so it gets the same one again
  EXPECT_EQ(feed(acknowledger, DATA_FRAME_0_RETRANSMITTED), 1);
  EXPECT_EQ(feed(acknowledger, DATA_FRAME_1), 2);
}

TEST(AshAcknowledger, RstackRestartsNumbering) {
  AshAcknowledger acknowledger;
  EXPECT_EQ(feed(acknowledger, DATA_FRAME_0), 1);
  EXPECT_EQ(feed(acknowledger, RSTACK), -1);
  EXPECT_EQ(feed(acknowledger, DATA_FRAME_0), 1);
}

TEST(AshAcknowledger, IgnoresShortDataFrames) {
  AshAcknowledger acknowledger;
  EXPECT_EQ(feed(acknowledger, SHORT_DATA_FRAME_0), -1);
  EXPECT_EQ(feed(acknowledger, MIN_DATA_FRAME_0), 1);
}

TEST(AshAcknowledger, SyncsToFirstFrameMidSession) {
  AshAcknowledger acknowledger;
  // The NCP is retransmitting a frame nobody acknowledged before the tap was watching
  EXPECT_EQ(feed(acknowledger, DATA_FRAME_5_RETRANSMITTED), 6);
  EXPECT_EQ(feed(acknowledger, DATA_FRAME_6), 7);
}

TEST(AshAcknowledger, ResetForgetsNumbering) {
  AshAcknowledger acknowledger;
  EXPECT_EQ(feed(acknowledger, DATA_FRAME_0), 1);
  acknowledger.reset();
  EXPECT_EQ(feed(acknowledger, DATA_FRAME_5), 6);
}

}  // namespace esphome::ezsp_proxy_tap::testing
