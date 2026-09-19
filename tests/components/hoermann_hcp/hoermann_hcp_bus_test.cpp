#include <gtest/gtest.h>

#include "common.h"

// The tests in hoermann_hcp_test.cpp call the register handlers directly, so they say nothing about the
// announcement reaching a real hub. These drive it through ModbusServerHub and a fake wire instead.
namespace esphome::hoermann_hcp::testing {

// The whole exchange through the real hub: the announcement serves the bus itself, the controller's
// acknowledgement arrives as a 0x17 request, is taken, answered on the read half, and the announcement
// reports success.
TEST(HoermannHcpBus, AcknowledgementArrivesThroughTheHub) {
  BusFixture f;
  f.wire.inject_poll(0x02, {0x8104, 0x1900, 0x0200}, 8);

  EXPECT_TRUE(f.door.announce_pause());
  ASSERT_GE(f.wire.written.size(), 3u + 16u);
  EXPECT_EQ(f.wire.reply_register(0), 0x0100);
  EXPECT_EQ(f.wire.reply_register(1), 0x04FD);  // the transfer acknowledged
}

// The first poll during an announcement carries the pause on the wire, not the state.
TEST(HoermannHcpBus, PauseGoesOutOnTheWire) {
  BusFixture f;
  f.door.pause_state_ = PauseState::PAUSE_STATE_WAITING_FOR_ACK;
  f.wire.inject_poll(0x02, {0x3400, 0x0000}, 8);

  f.hub.loop();
  ASSERT_GE(f.wire.written.size(), 3u + 16u);
  EXPECT_EQ(f.wire.reply_register(0), 0x3400);
  EXPECT_EQ(f.wire.reply_register(1), 0x0029);
  EXPECT_EQ(f.wire.reply_register(2), 0x0002);
}

// With the controller polling but never acknowledging, the announcement gives up on time and the device is
// answering with the state again afterwards.
TEST(HoermannHcpBus, UnacknowledgedAnnouncementGivesUpAndRecovers) {
  BusFixture f;
  f.wire.inject_poll(0x02, {0x0000, 0x0000}, 8);

  EXPECT_FALSE(f.door.announce_pause());
  ASSERT_GE(f.wire.written.size(), 3u + 16u);
  EXPECT_EQ(f.wire.reply_register(1), 0x0029);  // what the poll during the announcement got

  f.wire.written.clear();
  f.wire.inject_poll(0x02, {0x0000, 0x0000}, 8);
  f.hub.loop();
  ASSERT_GE(f.wire.written.size(), 3u + 16u);
  EXPECT_EQ(f.wire.reply_register(1), 0x0001);
}

// A second announcement while one is running would reset the state machine under the first one's feet.
TEST(HoermannHcpBus, ASecondAnnouncementIsRefusedWhileOneRuns) {
  BusFixture f;
  f.door.announcing_ = true;

  EXPECT_FALSE(f.door.announce_pause());
  EXPECT_EQ(f.door.pause_state_, PauseState::PAUSE_STATE_IDLE);
}

}  // namespace esphome::hoermann_hcp::testing
