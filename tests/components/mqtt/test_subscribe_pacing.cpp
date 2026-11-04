#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include "esphome/components/mqtt/mqtt_subscription_fingerprint.h"

namespace esphome::mqtt::testing {

// The inbound event pool of the ESP32 backend
static constexpr size_t POOL_SIZE = 31;

TEST(SubscribeFits, KeepsTwoSlotsPerUnansweredSubscribe) {
  EXPECT_TRUE(subscribe_fits(0, 0, 2));
  EXPECT_FALSE(subscribe_fits(0, 0, 1));
  EXPECT_FALSE(subscribe_fits(0, 0, 0));
  // 14 unanswered plus the new one need 30 slots
  EXPECT_TRUE(subscribe_fits(14, 0, POOL_SIZE));
  EXPECT_FALSE(subscribe_fits(15, 0, POOL_SIZE));
  EXPECT_TRUE(subscribe_fits(15, 1, POOL_SIZE));
}

TEST(SubscribeFits, AnsweredSubscribesFreeTheWindow) {
  EXPECT_FALSE(subscribe_fits(40, 30, 20));
  EXPECT_TRUE(subscribe_fits(40, 31, 20));
  EXPECT_TRUE(subscribe_fits(40, 40, 2));
}

TEST(SubscribeFits, ExtraSubacksCountAsNoneUnanswered) {
  // A SUBACK from an earlier connection counted after the reset
  EXPECT_TRUE(subscribe_fits(0, 1, 2));
  EXPECT_FALSE(subscribe_fits(0, 1, 1));
}

TEST(SubscribeFits, CountersWrap) {
  // 15 unanswered across the 16-bit rollover still fill the window
  EXPECT_FALSE(subscribe_fits(9, 65530, POOL_SIZE));
  EXPECT_TRUE(subscribe_fits(8, 65530, POOL_SIZE));
}

TEST(SubackCounter, ReceivedCountsEverySuback) {
  SubackCounter subacks;
  EXPECT_EQ(subacks.received(), 0);
  subacks.add(0);
  subacks.add(SUBACK_FAILURE);
  EXPECT_EQ(subacks.received(), 2);
  subacks.reset();
  EXPECT_EQ(subacks.received(), 0);
}

struct BurstResult {
  size_t delivered{0};
  size_t dropped{0};
};

/// Connect burst model: the task answers every sent SUBSCRIBE (SUBACK, then one retained message) before the loop
/// drains the queue; split_last runs the loop between the last SUBACK and its message.
static BurstResult run_burst(uint16_t topics, bool paced, bool queue_acks, bool split_last) {
  BurstResult result;
  size_t queued = 0;
  uint16_t sent = 0;
  uint16_t answered = 0;
  bool retained_due = false;
  auto push = [&]() {
    if (queued < POOL_SIZE) {
      queued++;
    } else {
      result.dropped++;
    }
  };
  for (int pass = 0; pass < 10000 && (sent < topics || queued != 0 || retained_due); pass++) {
    result.delivered += queued;
    queued = 0;
    while (sent < topics && (!paced || subscribe_fits(sent, answered, POOL_SIZE - queued)))
      sent++;
    if (retained_due) {
      retained_due = false;
      push();
    }
    for (; answered < sent; answered++) {
      if (queue_acks)
        push();
      if (split_last && answered + 1 == sent) {
        retained_due = true;
      } else {
        push();
      }
    }
  }
  return result;
}

TEST(SubscribePacing, UnpacedBurstOverflowsThePool) {
  // 49 is what an ESP32-C3 with 40 retained topics lost before acks were kept out of the queue
  EXPECT_EQ(run_burst(40, false, true, false).dropped, 49);
  EXPECT_EQ(run_burst(40, false, false, false).dropped, 9);
}

TEST(SubscribePacing, PacedBurstDeliversEveryRetainedMessage) {
  for (uint16_t topics : {1, 15, 31, 40, 100, 250}) {
    for (bool split_last : {false, true}) {
      const BurstResult result = run_burst(topics, true, false, split_last);
      EXPECT_EQ(result.dropped, 0) << topics << " topics, split_last=" << split_last;
      EXPECT_EQ(result.delivered, topics) << topics << " topics, split_last=" << split_last;
    }
  }
}

}  // namespace esphome::mqtt::testing
