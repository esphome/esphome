#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <vector>

#include "esphome/components/bl0942/bl0942.h"
#include "esphome/components/sensor/sensor.h"
#include "../common.h"

namespace esphome::bl0942::testing {

namespace {

using Bytes = std::vector<uint8_t>;

// A full packet for the given voltage register value, as the chip at address 0 sends it: the
// checksum is the sum of the read command and the first 22 bytes, inverted.
Bytes packet(uint32_t v_rms) {
  Bytes bytes(23, 0x00);
  bytes[0] = 0x55;
  bytes[4] = (uint8_t) v_rms;
  bytes[5] = (uint8_t) (v_rms >> 8);
  bytes[6] = (uint8_t) (v_rms >> 16);
  uint8_t sum = 0x58;
  for (size_t i = 0; i != 22; i++)
    sum += bytes[i];
  bytes[22] = sum ^ 0xFF;
  return bytes;
}

// The voltage register value which reads as this many volts with the default reference.
constexpr uint32_t volts(float v) { return (uint32_t) (v * BL0942_UREF); }

class BL0942FrameTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sut_.set_uart_parent(&this->uart_);
    this->sut_.set_voltage_sensor(&this->voltage_);
  }

  void receive_(const Bytes &bytes) {
    this->uart_.rx.insert(this->uart_.rx.end(), bytes.begin(), bytes.end());
    this->sut_.loop();
  }

  FakeUART uart_;
  BL0942 sut_;
  sensor::Sensor voltage_;
};

TEST_F(BL0942FrameTest, WholePacketIsPublished) {
  this->receive_(packet(volts(230)));
  EXPECT_NEAR(this->voltage_.state, 230.0f, 0.01f);
}

// The bytes of a packet come in over several passes of the loop. Nothing is published, and nothing
// is left waiting in the UART, until the last of them has arrived.
TEST_F(BL0942FrameTest, PacketInPiecesIsPublishedWhenComplete) {
  const Bytes bytes = packet(volts(240));
  this->receive_(Bytes(bytes.begin(), bytes.begin() + 1));
  this->receive_(Bytes(bytes.begin() + 1, bytes.begin() + 10));
  EXPECT_FALSE(this->voltage_.has_state());
  EXPECT_TRUE(this->uart_.rx.empty());
  this->receive_(Bytes(bytes.begin() + 10, bytes.end()));
  EXPECT_NEAR(this->voltage_.state, 240.0f, 0.01f);
}

TEST_F(BL0942FrameTest, NoiseBeforeThePacketIsSkipped) {
  this->receive_({0x00, 0xFF, 0x12});
  this->receive_(packet(volts(231)));
  EXPECT_NEAR(this->voltage_.state, 231.0f, 0.01f);
}

// A stray byte which happens to be the header value starts a packet which fails its checksum. The
// real packet began inside it, and is found there.
TEST_F(BL0942FrameTest, PacketBehindAFalseHeaderIsFound) {
  Bytes bytes{0x55, 0x01, 0x02};
  const Bytes real = packet(volts(232));
  bytes.insert(bytes.end(), real.begin(), real.end());
  this->receive_(bytes);
  EXPECT_NEAR(this->voltage_.state, 232.0f, 0.01f);
}

TEST_F(BL0942FrameTest, BadChecksumIsNotPublished) {
  Bytes bytes = packet(volts(233));
  bytes[22] ^= 0x01;
  this->receive_(bytes);
  EXPECT_FALSE(this->voltage_.has_state());
}

// The rest of a rejected packet is searched for a header, so a header value among its bytes leaves a
// partial packet behind. The next request drops it.
TEST_F(BL0942FrameTest, PacketAfterARejectedOneIsPublished) {
  Bytes bad = packet(volts(235));
  bad[10] = 0x55;
  this->receive_(bad);
  EXPECT_FALSE(this->voltage_.has_state());
  this->sut_.update();
  this->receive_(packet(volts(236)));
  EXPECT_NEAR(this->voltage_.state, 236.0f, 0.01f);
}

// A packet which stopped part of the way through must not take the first bytes of the answer to
// the next request as its own.
TEST_F(BL0942FrameTest, PartialPacketIsForgottenAtTheNextRequest) {
  const Bytes stale = packet(volts(100));
  this->receive_(Bytes(stale.begin(), stale.begin() + 12));
  this->sut_.update();
  this->receive_(packet(volts(234)));
  EXPECT_NEAR(this->voltage_.state, 234.0f, 0.01f);
}

}  // namespace

}  // namespace esphome::bl0942::testing
