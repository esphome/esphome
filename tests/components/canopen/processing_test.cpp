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
