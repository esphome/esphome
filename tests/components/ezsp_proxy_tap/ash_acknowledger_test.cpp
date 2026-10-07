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

}  // namespace esphome::ezsp_proxy_tap::testing
