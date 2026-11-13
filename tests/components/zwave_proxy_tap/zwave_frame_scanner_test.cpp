#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/zwave_proxy_tap/zwave_frame_scanner.h"

namespace esphome::zwave_proxy_tap::testing {

namespace {

// GetControllerVersion request: SOF, LENGTH 3, REQ, 0x15, checksum
const std::vector<uint8_t> NO_PAYLOAD_FRAME = {0x01, 0x03, 0x00, 0x15, 0xE9};
// GetNetworkIds response for home ID 0xC1F1E3E5, node 1
const std::vector<uint8_t> PAYLOAD_FRAME = {0x01, 0x08, 0x01, 0x20, 0xC1, 0xF1, 0xE3, 0xE5, 0x01, 0xE1};

std::vector<uint8_t> concat(std::vector<uint8_t> a, const std::vector<uint8_t> &b) {
  a.insert(a.end(), b.begin(), b.end());
  return a;
}

// Returns the indexes of the bytes that completed a frame owed an ACK
std::vector<size_t> feed(ZWaveFrameScanner &scanner, const std::vector<uint8_t> &bytes, uint32_t now = 0) {
  std::vector<size_t> acks;
  for (size_t i = 0; i < bytes.size(); i++) {
    if (scanner.feed(bytes[i], now))
      acks.push_back(i);
  }
  return acks;
}

}  // namespace

TEST(ZWaveFrameScanner, AcceptsFrameWithoutPayload) {
  ZWaveFrameScanner scanner;
  EXPECT_EQ(feed(scanner, NO_PAYLOAD_FRAME), (std::vector<size_t>{4}));
}

TEST(ZWaveFrameScanner, AcceptsFrameWithPayload) {
  ZWaveFrameScanner scanner;
  EXPECT_EQ(feed(scanner, PAYLOAD_FRAME), (std::vector<size_t>{9}));
}

TEST(ZWaveFrameScanner, AcceptsBackToBackFrames) {
  ZWaveFrameScanner scanner;
  EXPECT_EQ(feed(scanner, concat(NO_PAYLOAD_FRAME, PAYLOAD_FRAME)), (std::vector<size_t>{4, 14}));
}

TEST(ZWaveFrameScanner, IgnoresBytesBetweenFrames) {
  ZWaveFrameScanner scanner;
  // ACK, NAK, CAN and noise between frames
  EXPECT_EQ(feed(scanner, concat({0x06, 0x15, 0x18, 0xFF, 0x00}, NO_PAYLOAD_FRAME)), (std::vector<size_t>{9}));
}

TEST(ZWaveFrameScanner, RejectsBadChecksum) {
  ZWaveFrameScanner scanner;
  std::vector<uint8_t> corrupt = PAYLOAD_FRAME;
  corrupt[5] ^= 0x01;
  EXPECT_TRUE(feed(scanner, corrupt).empty());
  EXPECT_EQ(feed(scanner, NO_PAYLOAD_FRAME), (std::vector<size_t>{4}));
}

TEST(ZWaveFrameScanner, SofInsidePayloadIsData) {
  ZWaveFrameScanner scanner;
  // Node ID 1 is an SOF byte inside the payload, and the frame still ends at its LENGTH
  EXPECT_EQ(feed(scanner, concat(PAYLOAD_FRAME, NO_PAYLOAD_FRAME)), (std::vector<size_t>{9, 14}));
}

TEST(ZWaveFrameScanner, ShortLengthConsumesLengthBytes) {
  ZWaveFrameScanner scanner;
  // LENGTH 0 ends the frame at once and LENGTH 2 swallows two bytes. Neither is ACKed, even when
  // the bytes would make the checksum valid.
  EXPECT_TRUE(feed(scanner, {0x01, 0x00}).empty());
  EXPECT_TRUE(feed(scanner, {0x01, 0x02, 0x00, 0xFD}).empty());
  EXPECT_EQ(feed(scanner, NO_PAYLOAD_FRAME), (std::vector<size_t>{4}));
  // The byte after LENGTH 1 is consumed even when it is an SOF
  EXPECT_TRUE(feed(scanner, {0x01, 0x01, 0x01, 0x03, 0x00, 0x15, 0xE9}).empty());
}

TEST(ZWaveFrameScanner, AbandonsStalledFrame) {
  ZWaveFrameScanner scanner;
  EXPECT_TRUE(feed(scanner, {0x01, 0x08, 0x01}, 0).empty());
  // The rest of the frame arrives too late: the bytes before the next SOF are noise
  EXPECT_EQ(feed(scanner, concat({0x20, 0xC1}, NO_PAYLOAD_FRAME), ZWAVE_FRAME_TIMEOUT_MS + 1),
            (std::vector<size_t>{6}));
}

TEST(ZWaveFrameScanner, KeepsFrameWithinTimeout) {
  ZWaveFrameScanner scanner;
  EXPECT_TRUE(feed(scanner, {0x01, 0x03, 0x00}, 0).empty());
  EXPECT_EQ(feed(scanner, {0x15, 0xE9}, ZWAVE_FRAME_TIMEOUT_MS), (std::vector<size_t>{1}));
}

TEST(ZWaveFrameScanner, ResetDiscardsPartialFrame) {
  ZWaveFrameScanner scanner;
  EXPECT_TRUE(feed(scanner, {0x01, 0x08, 0x01}).empty());
  scanner.reset();
  EXPECT_EQ(feed(scanner, NO_PAYLOAD_FRAME), (std::vector<size_t>{4}));
}

}  // namespace esphome::zwave_proxy_tap::testing
