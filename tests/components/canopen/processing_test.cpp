#include <gtest/gtest.h>

#include <vector>

#include "fake_canbus.h"

// When a hub processes received frames and retries the frames the controller refused.
namespace esphome::canopen::testing {

// A node processes received frames right away, but queued frames are retried only from loop(): on ESP32 each refused
// send can block.
TEST(CanopenProcessing, NodeRetriesQueuedFramesOnlyFromLoop) {
  FakeCanbus bus;
  TestHub hub(&bus);
  hub.setup();
  ASSERT_FALSE(hub.is_failed());
  hub.loop();
  bus.busy = true;
  ASSERT_TRUE(hub.write_sdo<uint8_t>(0x21, 0x6060, 0, 1));
  hub.loop();
  bus.busy = false;
  hub.frame(0x701, {0x05});
  EXPECT_EQ(bus.count(0x621), 0u);
  hub.loop();
  EXPECT_EQ(bus.count(0x621), 1u);
}

static void collect(void *context, const SDOReply &reply) {
  static_cast<std::vector<SDOReply> *>(context)->push_back(reply);
}

// loop() flushes queued frames once, then restarts the SDO response timer if the request left. CO_process() doesn't
// flush a second time: a request sent there would keep the time it waited as part of its response timeout.
TEST(CanopenProcessing, SdoTimeoutCountsFromTheFlushThatSentTheRequest) {
  FakeCanbus bus;
  TestHub hub(&bus);
  hub.set_heartbeat_interval(0);  // only the request is queued
  hub.set_response_timeout(500);
  hub.setup();
  ASSERT_FALSE(hub.is_failed());
  hub.loop();  // the boot-up message
  std::vector<SDOReply> replies;
  bus.busy = true;
  ASSERT_TRUE(hub.read_sdo<uint16_t>(0x21, 0x6041, 0, collect, &replies));
  hub.loop();
  advance_loop_clock(400);
  hub.loop();  // 400 ms queued
  // the next send is refused, the one after it taken
  int sends = 0;
  bus.on_send = [&bus, &sends](uint32_t /*can_id*/) {
    if (++sends == 2)
      bus.busy = false;
  };
  advance_loop_clock(10);
  hub.loop();
  EXPECT_EQ(bus.count(0x621), 0u);  // not sent again in the same loop
  hub.loop();
  ASSERT_EQ(bus.count(0x621), 1u);
  bus.on_send = nullptr;
  advance_loop_clock(300);
  hub.loop();
  EXPECT_TRUE(replies.empty());  // 300 ms after it left, though 710 ms after it was made
  hub.frame(0x5A1, {0x4B, 0x41, 0x60, 0, 0x37, 0x02, 0, 0});
  ASSERT_EQ(replies.size(), 1u);
  EXPECT_TRUE(replies[0].ok());
}

// CiA 301: a STOPPED node sends no SDO frames. Running and queued requests end with REJECTED, new ones are rejected,
// and requests work again once the node leaves STOPPED.
TEST(CanopenProcessing, StoppedNodeRejectsSdoRequests) {
  FakeCanbus bus;
  TestHub hub(&bus);
  hub.setup();
  ASSERT_FALSE(hub.is_failed());
  hub.loop();
  std::vector<SDOReply> replies;
  ASSERT_TRUE(hub.read_sdo<uint16_t>(0x21, 0x6041, 0, collect, &replies));
  hub.loop();
  ASSERT_EQ(bus.count(0x621), 1u);
  ASSERT_TRUE(hub.read_sdo<uint16_t>(0x22, 0x6041, 0, collect, &replies));  // queued behind it
  ASSERT_TRUE(hub.set_nmt_state(NMTState::NMT_STATE_STOPPED));
  hub.loop();
  hub.loop();
  ASSERT_EQ(hub.get_nmt_state(), NMTState::NMT_STATE_STOPPED);
  ASSERT_EQ(replies.size(), 2u);
  EXPECT_EQ(replies[0].result, SDOResult::SDO_RESULT_REJECTED);
  EXPECT_EQ(replies[1].result, SDOResult::SDO_RESULT_REJECTED);
  EXPECT_FALSE(hub.read_sdo<uint16_t>(0x23, 0x6041, 0, collect, &replies));
  ASSERT_EQ(replies.size(), 3u);
  EXPECT_EQ(replies[2].result, SDOResult::SDO_RESULT_REJECTED);
  hub.frame(0x5A1, {0x4B, 0x41, 0x60, 0, 0x37, 0x02, 0, 0});  // a late answer is ignored
  hub.loop();
  EXPECT_EQ(replies.size(), 3u);
  EXPECT_EQ(bus.count(0x621), 1u);
  EXPECT_EQ(bus.count(0x622), 0u);
  EXPECT_EQ(bus.count(0x623), 0u);

  ASSERT_TRUE(hub.set_nmt_state(NMTState::NMT_STATE_OPERATIONAL));
  hub.loop();
  hub.loop();
  ASSERT_TRUE(hub.read_sdo<uint16_t>(0x22, 0x6041, 0, collect, &replies));
  hub.loop();
  EXPECT_EQ(bus.count(0x622), 1u);
}

// A client-only hub has nothing to do between requests: its loop is off until the next one.
TEST_F(SdoTest, ClientOnlyLoopIdlesBetweenRequests) {
  this->hub_.loop();
  EXPECT_TRUE(this->hub_.is_idle());
  ASSERT_TRUE(this->hub_.read_sdo<uint16_t>(0x21, 0x6041, 0, on_reply, this));
  EXPECT_FALSE(this->hub_.is_idle());
  this->hub_.loop();
  this->hub_.frame(0x5A1, {0x4B, 0x41, 0x60, 0, 0x37, 0x02, 0, 0});
  ASSERT_EQ(this->replies_.size(), 1u);
  this->hub_.loop();
  EXPECT_TRUE(this->hub_.is_idle());
}

// A node always has work (heartbeat, PDO timers).
TEST(CanopenProcessing, NodeLoopStaysOn) {
  FakeCanbus bus;
  TestHub hub(&bus);
  hub.setup();
  ASSERT_FALSE(hub.is_failed());
  hub.loop();
  hub.loop();
  EXPECT_FALSE(hub.is_idle());
}

}  // namespace esphome::canopen::testing
