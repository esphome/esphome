#include <gtest/gtest.h>
#include "esphome/components/packet_interface/listen_before_talk.h"

namespace esphome::packet_interface::lbt_testing {

static constexpr uint32_t START = 10000;
static constexpr uint32_t MAX_BUSY = ListenBeforeTalk::DEFAULT_MAX_BUSY_MS;

TEST(ListenBeforeTalkTest, SendsWhenNotBusy) {
  ListenBeforeTalk lbt;
  EXPECT_TRUE(lbt.may_send(false, START, START));
}

TEST(ListenBeforeTalkTest, RefusesWhileBusyUpToLimit) {
  ListenBeforeTalk lbt;
  EXPECT_FALSE(lbt.may_send(true, START, START));
  // Refused every 50 ms, as the reliability layer retries
  uint32_t now = START;
  while (now - START < MAX_BUSY - 50) {
    now += 50;
    EXPECT_FALSE(lbt.may_send(true, now, now));
  }
  now += 50;
  EXPECT_TRUE(lbt.may_send(true, now, now));
  EXPECT_EQ(lbt.busy_for(now), MAX_BUSY);
}

TEST(ListenBeforeTalkTest, BusyFlagThatNeverClearsReleasedForSlowSends) {
  ListenBeforeTalk lbt;
  // A send every 10 s, as session keep-alives, always finding the radio busy
  EXPECT_FALSE(lbt.may_send(true, START, START));
  EXPECT_TRUE(lbt.may_send(true, START + 10000, START + 10000));
}

TEST(ListenBeforeTalkTest, FreeRadioEndsBusyPeriod) {
  ListenBeforeTalk lbt;
  EXPECT_FALSE(lbt.may_send(true, START, START));
  EXPECT_TRUE(lbt.may_send(false, START + 1500, START + 1500));
  // A new busy period starts, so the earlier one does not count
  EXPECT_FALSE(lbt.may_send(true, START + MAX_BUSY + 100, START + MAX_BUSY + 100));
  EXPECT_EQ(lbt.busy_for(START + MAX_BUSY + 100), 0);
}

TEST(ListenBeforeTalkTest, EndBusyStartsNewPeriod) {
  ListenBeforeTalk lbt;
  EXPECT_FALSE(lbt.may_send(true, START, START));
  // The link saw the radio free between sends, so a later reception is a new busy period
  lbt.end_busy();
  EXPECT_FALSE(lbt.may_send(true, START + MAX_BUSY + 100, START + MAX_BUSY + 100));
  EXPECT_EQ(lbt.busy_for(START + MAX_BUSY + 100), 0);
}

TEST(ListenBeforeTalkTest, LimitCanBeLonger) {
  ListenBeforeTalk lbt;
  lbt.set_max_busy(8000);
  EXPECT_FALSE(lbt.may_send(true, START, START));
  EXPECT_FALSE(lbt.may_send(true, START + 7999, START + 7999));
  EXPECT_TRUE(lbt.may_send(true, START + 8000, START + 8000));
}

static constexpr uint32_t TURNAROUND = 250;

TEST(ListenBeforeTalkTest, SendInLaterPassWaitsForTurnaround) {
  ListenBeforeTalk lbt;
  lbt.set_turnaround(TURNAROUND);
  lbt.sent(START + 300, START);
  EXPECT_FALSE(lbt.may_send(false, START + 320, START + 320));
  EXPECT_FALSE(lbt.may_send(false, START + 300 + TURNAROUND - 1, START + 520));
  EXPECT_TRUE(lbt.may_send(false, START + 300 + TURNAROUND, START + 540));
}

TEST(ListenBeforeTalkTest, SendInSamePassNotHeld) {
  ListenBeforeTalk lbt;
  lbt.set_turnaround(TURNAROUND);
  // The next fragment of the same packet
  lbt.sent(START + 300, START);
  EXPECT_TRUE(lbt.may_send(false, START + 301, START));
}

TEST(ListenBeforeTalkTest, ReceptionEndsTurnaround) {
  ListenBeforeTalk lbt;
  lbt.set_turnaround(TURNAROUND);
  lbt.sent(START + 300, START);
  lbt.received();
  EXPECT_TRUE(lbt.may_send(false, START + 320, START + 320));
}

TEST(ListenBeforeTalkTest, NoTurnaroundByDefault) {
  ListenBeforeTalk lbt;
  lbt.sent(START + 300, START);
  EXPECT_TRUE(lbt.may_send(false, START + 320, START + 320));
}

}  // namespace esphome::packet_interface::lbt_testing
