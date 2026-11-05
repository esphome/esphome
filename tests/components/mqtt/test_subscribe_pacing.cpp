#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>

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

TEST(SubscribeFits, QueuedAcksKeepThreeSlots) {
  // 9 unanswered plus the new one need 30 slots
  EXPECT_TRUE(subscribe_fits(9, 0, POOL_SIZE, INBOUND_SLOTS_PER_SUBSCRIBE_WITH_ACK));
  EXPECT_FALSE(subscribe_fits(10, 0, POOL_SIZE, INBOUND_SLOTS_PER_SUBSCRIBE_WITH_ACK));
  EXPECT_FALSE(subscribe_fits(15, 0, POOL_SIZE - 1, INBOUND_SLOTS_PER_SUBSCRIBE_WITH_ACK));
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

/// Connect burst model; the loop may subscribe after a SUBACK is counted but before its retained message is queued.
static BurstResult run_burst(uint16_t topics, bool paced, bool queue_acks) {
  BurstResult result;
  size_t queued = 0;
  uint16_t sent = 0;
  uint16_t answered = 0;
  const size_t slots = queue_acks ? INBOUND_SLOTS_PER_SUBSCRIBE_WITH_ACK : INBOUND_SLOTS_PER_SUBSCRIBE;
  auto push = [&]() {
    if (queued < POOL_SIZE) {
      queued++;
    } else {
      result.dropped++;
    }
  };
  auto subscribe = [&]() {
    while (sent < topics && (!paced || subscribe_fits(sent, answered, POOL_SIZE - queued, slots)))
      sent++;
  };
  for (int pass = 0; pass < 10000 && (sent < topics || queued != 0); pass++) {
    result.delivered += queued;
    queued = 0;
    subscribe();
    while (answered < sent) {
      answered++;
      if (queue_acks)
        push();
      subscribe();
      push();
    }
  }
  return result;
}

TEST(SubscribePacing, UnpacedBurstOverflowsThePool) {
  // 49 is what an ESP32-C3 with 40 retained topics lost before acks were kept out of the queue
  EXPECT_EQ(run_burst(40, false, true).dropped, 49);
  EXPECT_EQ(run_burst(40, false, false).dropped, 9);
}

TEST(SubscribePacing, PacedBurstDeliversEveryRetainedMessage) {
  for (uint16_t topics : {1, 15, 31, 40, 100, 250}) {
    const BurstResult result = run_burst(topics, true, false);
    EXPECT_EQ(result.dropped, 0) << topics << " topics";
    EXPECT_EQ(result.delivered, topics) << topics << " topics";
  }
}

TEST(SubscribePacing, PacedBurstWithQueuedAcksDeliversEveryRetainedMessage) {
  for (uint16_t topics : {1, 15, 16, 31, 40, 100, 250}) {
    const BurstResult result = run_burst(topics, true, true);
    EXPECT_EQ(result.dropped, 0) << topics << " topics";
    EXPECT_EQ(result.delivered, topics * 2u) << topics << " topics";
  }
}

}  // namespace esphome::mqtt::testing
