#include <gtest/gtest.h>
#include "esphome/components/packet_interface/listen_before_talk.h"

namespace esphome::packet_interface::lbt_testing {

static constexpr uint32_t START = 10000;
static constexpr uint32_t MAX_BUSY = ListenBeforeTalk::DEFAULT_MAX_BUSY_MS;

TEST(ListenBeforeTalkTest, SendsWhenNotBusy) {
  ListenBeforeTalk lbt;
  EXPECT_TRUE(lbt.may_send(false, START));
}

TEST(ListenBeforeTalkTest, RefusesWhileBusyUpToLimit) {
  ListenBeforeTalk lbt;
  EXPECT_FALSE(lbt.may_send(true, START));
  // Refused every 50 ms, as the reliability layer retries
  uint32_t now = START;
  while (now - START < MAX_BUSY - 50) {
    now += 50;
    EXPECT_FALSE(lbt.may_send(true, now));
  }
  now += 50;
  EXPECT_TRUE(lbt.may_send(true, now));
  EXPECT_EQ(lbt.busy_for(now), MAX_BUSY);
}

TEST(ListenBeforeTalkTest, BusyFlagThatNeverClearsReleasedForSlowSends) {
  ListenBeforeTalk lbt;
  // A send every 10 s, as session keep-alives, always finding the radio busy
  EXPECT_FALSE(lbt.may_send(true, START));
  EXPECT_TRUE(lbt.may_send(true, START + 10000));
}

TEST(ListenBeforeTalkTest, FreeRadioEndsBusyPeriod) {
  ListenBeforeTalk lbt;
  EXPECT_FALSE(lbt.may_send(true, START));
  EXPECT_TRUE(lbt.may_send(false, START + 1500));
  // A new busy period starts, so the earlier one does not count
  EXPECT_FALSE(lbt.may_send(true, START + MAX_BUSY + 100));
  EXPECT_EQ(lbt.busy_for(START + MAX_BUSY + 100), 0);
}

TEST(ListenBeforeTalkTest, EndBusyStartsNewPeriod) {
  ListenBeforeTalk lbt;
  EXPECT_FALSE(lbt.may_send(true, START));
  // The link saw the radio free between sends, so a later reception is a new busy period
  lbt.end_busy();
  EXPECT_FALSE(lbt.may_send(true, START + MAX_BUSY + 100));
  EXPECT_EQ(lbt.busy_for(START + MAX_BUSY + 100), 0);
}

TEST(ListenBeforeTalkTest, LimitCanBeLonger) {
  ListenBeforeTalk lbt;
  lbt.set_max_busy(8000);
  EXPECT_FALSE(lbt.may_send(true, START));
  EXPECT_FALSE(lbt.may_send(true, START + 7999));
  EXPECT_TRUE(lbt.may_send(true, START + 8000));
}

}  // namespace esphome::packet_interface::lbt_testing
