#include <gtest/gtest.h>

#include <initializer_list>
#include <utility>

#include "esphome/components/mqtt/mqtt_subscription_fingerprint.h"

namespace esphome::mqtt::testing {

static constexpr uint32_t SEED = 0x12345678;

static uint32_t fingerprint(std::initializer_list<std::pair<const char *, uint8_t>> subscriptions) {
  uint32_t value = SEED;
  for (const auto &[topic, qos] : subscriptions)
    value = add_subscription_fingerprint(value, topic, qos);
  return value;
}

TEST(SubscriptionFingerprint, OrderDoesNotMatter) {
  EXPECT_EQ(fingerprint({{"a/set", 0}, {"b/set", 1}, {"c/set", 0}}),
            fingerprint({{"c/set", 0}, {"a/set", 0}, {"b/set", 1}}));
}

TEST(SubscriptionFingerprint, TopicQosAndCountChangeIt) {
  const uint32_t base = fingerprint({{"a/set", 0}, {"b/set", 0}});
  EXPECT_NE(base, fingerprint({{"a/set", 0}, {"b/cmd", 0}}));
  EXPECT_NE(base, fingerprint({{"a/set", 0}, {"b/set", 1}}));
  EXPECT_NE(base, fingerprint({{"a/set", 0}}));
  // Duplicates do not cancel each other out
  EXPECT_NE(base, fingerprint({{"a/set", 0}, {"b/set", 0}, {"b/set", 0}, {"b/set", 0}}));
}

TEST(SubscriptionFingerprint, ChangedSetSubscribesAgain) {
  const uint32_t trusted = fingerprint({{"a/set", 0}, {"b/set", 1}});
  EXPECT_TRUE(session_has_subscriptions(true, trusted, fingerprint({{"b/set", 1}, {"a/set", 0}})));
  EXPECT_FALSE(session_has_subscriptions(true, trusted, fingerprint({{"a/set", 0}, {"c/set", 1}})));
}

TEST(SubscriptionFingerprint, NewSessionSubscribesAgain) {
  const uint32_t trusted = fingerprint({{"a/set", 0}});
  EXPECT_FALSE(session_has_subscriptions(false, trusted, trusted));
}

TEST(SubscriptionFingerprint, UntrustedNeverMatches) {
  EXPECT_FALSE(session_has_subscriptions(true, UNTRUSTED_SESSION_FINGERPRINT, UNTRUSTED_SESSION_FINGERPRINT));
}

TEST(SubscriptionFingerprint, RefusedSubackIsNotGranted) {
  EXPECT_TRUE(subscribe_granted(0));
  EXPECT_TRUE(subscribe_granted(2));
  EXPECT_FALSE(subscribe_granted(0x80));
  // MQTT 5 reason codes
  EXPECT_FALSE(subscribe_granted(0x87));
}

}  // namespace esphome::mqtt::testing
