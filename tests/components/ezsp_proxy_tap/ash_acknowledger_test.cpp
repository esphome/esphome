#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/ezsp_proxy_tap/ash_acknowledger.h"
#include "esphome/components/ezsp_proxy_tap/ash_protocol.h"

namespace esphome::ezsp_proxy_tap::testing {

namespace {

// Whole frames as they appear on the wire, each ending in its FLAG. RSTACK and ACK(1)
// are the examples in Silicon Labs UG101.
const std::vector<uint8_t> RSTACK = {0xC1, 0x02, 0x02, 0x9B, 0x7B, 0x7E};
const std::vector<uint8_t> DATA_FRAME_0 = {0x00, 0x43, 0x21, 0xA8, 0x50, 0x9B, 0x98, 0x7E};
const std::vector<uint8_t> DATA_FRAME_1 = {0x10, 0x43, 0x21, 0xA8, 0x50, 0x9F, 0xC2, 0x7E};
const std::vector<uint8_t> DATA_FRAME_0_RETRANSMITTED = {0x08, 0x43, 0x21, 0xA8, 0x50, 0x99, 0xB5, 0x7E};

void feed(AshAcknowledger &acknowledger, const std::vector<uint8_t> &bytes) {
  for (uint8_t byte : bytes)
    acknowledger.feed(byte);
}

}  // namespace

TEST(AshProtocol, AckFrameMatchesReference) {
  uint8_t frame[ASH_ACK_FRAME_MAX_SIZE];
  const size_t length = ash_build_ack_frame(frame, 1);
  EXPECT_EQ(std::vector<uint8_t>(frame, frame + length), (std::vector<uint8_t>{0x7E, 0x81, 0x60, 0x59, 0x7E}));
}

TEST(AshAcknowledger, AcknowledgesFramesInSequence) {
  AshAcknowledger acknowledger;
  uint8_t ack_num;

  feed(acknowledger, DATA_FRAME_0);
  ASSERT_TRUE(acknowledger.take_pending_ack(ack_num));
  EXPECT_EQ(ack_num, 1);
  EXPECT_FALSE(acknowledger.take_pending_ack(ack_num));

  // A repeat not marked as a retransmission is out of sequence
  feed(acknowledger, DATA_FRAME_0);
  EXPECT_FALSE(acknowledger.take_pending_ack(ack_num));

  // A retransmission means the NCP missed our ACK, so it gets the same one again
  feed(acknowledger, DATA_FRAME_0_RETRANSMITTED);
  ASSERT_TRUE(acknowledger.take_pending_ack(ack_num));
  EXPECT_EQ(ack_num, 1);

  feed(acknowledger, DATA_FRAME_1);
  ASSERT_TRUE(acknowledger.take_pending_ack(ack_num));
  EXPECT_EQ(ack_num, 2);
}

TEST(AshAcknowledger, RstackRestartsNumbering) {
  AshAcknowledger acknowledger;
  uint8_t ack_num;

  feed(acknowledger, DATA_FRAME_0);
  feed(acknowledger, RSTACK);
  EXPECT_FALSE(acknowledger.take_pending_ack(ack_num));

  feed(acknowledger, DATA_FRAME_0);
  ASSERT_TRUE(acknowledger.take_pending_ack(ack_num));
  EXPECT_EQ(ack_num, 1);
}

}  // namespace esphome::ezsp_proxy_tap::testing
