#include <gtest/gtest.h>
#include "esphome/components/packet_interface/listen_before_talk.h"

namespace esphome::packet_interface::lbt_testing {

static constexpr uint32_t START = 10000;

TEST(ListenBeforeTalkTest, SendsWhenNotBusy) {
  ListenBeforeTalk lbt;
  EXPECT_TRUE(lbt.may_send(false, START));
}

TEST(ListenBeforeTalkTest, RefusesWhileBusyUpToLimit) {
  ListenBeforeTalk lbt;
  EXPECT_FALSE(lbt.may_send(true, START));
  // Refused every 50 ms, as the reliability layer retries
  uint32_t now = START;
  while (now - START < ListenBeforeTalk::MAX_BUSY_MS - 50) {
    now += 50;
    EXPECT_FALSE(lbt.may_send(true, now));
  }
  now += 50;
  EXPECT_TRUE(lbt.may_send(true, now));
  EXPECT_EQ(lbt.busy_for(now), ListenBeforeTalk::MAX_BUSY_MS);
}

TEST(ListenBeforeTalkTest, GapStartsNewBusyPeriod) {
  ListenBeforeTalk lbt;
  EXPECT_FALSE(lbt.may_send(true, START));
  EXPECT_FALSE(lbt.may_send(true, START + 400));
  EXPECT_EQ(lbt.busy_for(START + 400), 400);
  uint32_t later = START + 400 + ListenBeforeTalk::BUSY_GAP_MS + 1;
  EXPECT_FALSE(lbt.may_send(true, later));
  EXPECT_EQ(lbt.busy_for(later), 0);
}

}  // namespace esphome::packet_interface::lbt_testing
