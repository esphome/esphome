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

TEST(SubscriptionFingerprint, SameSetInKeptSessionSkipsSubscribe) {
  const uint32_t trusted = fingerprint({{"a/set", 0}, {"b/set", 1}});
  EXPECT_TRUE(session_has_subscriptions(true, trusted, fingerprint({{"b/set", 1}, {"a/set", 0}})));
}

TEST(SubscriptionFingerprint, ChangedSetSubscribesAgain) {
  const uint32_t trusted = fingerprint({{"a/set", 0}, {"b/set", 1}});
  EXPECT_FALSE(session_has_subscriptions(true, trusted, fingerprint({{"a/set", 0}, {"c/set", 1}})));
}

TEST(SubscriptionFingerprint, NewSessionSubscribesAgain) {
  const uint32_t trusted = fingerprint({{"a/set", 0}});
  EXPECT_FALSE(session_has_subscriptions(false, trusted, trusted));
}

TEST(SubscriptionFingerprint, UntrustedNeverMatches) {
  EXPECT_FALSE(session_has_subscriptions(true, UNTRUSTED_SESSION_FINGERPRINT, UNTRUSTED_SESSION_FINGERPRINT));
}

TEST(SubscriptionFingerprint, InterruptedRebuildThenKeptSessionSubscribesAgain) {
  const uint32_t current = fingerprint({{"a/set", 0}, {"b/set", 1}});
  uint32_t trusted = current;
  // The broker lost the session, so every subscription is sent again
  ASSERT_FALSE(session_has_subscriptions(false, trusted, current));
  trusted = session_fingerprint_during_rebuild(false, trusted);
  // The link drops, or the device resets, after only some SUBSCRIBEs; the broker now keeps a partial session
  EXPECT_FALSE(session_has_subscriptions(true, trusted, current));
}

TEST(SubscriptionFingerprint, LostSessionWithSameSetIsTrustedAgainOnceAcknowledged) {
  const uint32_t current = fingerprint({{"a/set", 0}, {"b/set", 1}});
  uint32_t trusted = current;
  ASSERT_FALSE(session_has_subscriptions(false, trusted, current));
  trusted = session_fingerprint_during_rebuild(false, trusted);
  // Every SUBACK arrived, so the session holds the set again; nothing of this is kept in flash
  ASSERT_TRUE(fingerprint_save_allowed(2, 2, false));
  trusted = current;
  EXPECT_TRUE(session_has_subscriptions(true, trusted, current));
}

TEST(SubscriptionFingerprint, ColdBootSubscribesAgain) {
  // After a power loss the RTC copy fails its checksum and reads as untrusted, even with a kept session
  const uint32_t current = fingerprint({{"a/set", 0}});
  EXPECT_FALSE(session_has_subscriptions(true, UNTRUSTED_SESSION_FINGERPRINT, current));
}

TEST(SubscriptionFingerprint, DeepSleepWakeWithKeptSessionSkipsSubscribe) {
  // The RTC copy survived deep sleep, and the broker kept the session
  const uint32_t current = fingerprint({{"a/set", 0}, {"b/set", 1}});
  const uint32_t trusted = current;
  ASSERT_TRUE(session_has_subscriptions(true, trusted, current));
  EXPECT_EQ(session_fingerprint_during_rebuild(true, trusted), trusted);
}

TEST(SubscriptionFingerprint, RefusedSubackIsNotGranted) {
  EXPECT_TRUE(subscribe_granted(0));
  EXPECT_TRUE(subscribe_granted(2));
  EXPECT_FALSE(subscribe_granted(0x80));
  // MQTT 5 reason codes
  EXPECT_FALSE(subscribe_granted(0x87));
}

TEST(SubscriptionFingerprint, SavedOnlyOnceEverySubscribeIsAcknowledged) {
  EXPECT_TRUE(fingerprint_save_allowed(0, 0, false));
  EXPECT_TRUE(fingerprint_save_allowed(3, 3, false));
  // Sent but the link dropped before the broker answered
  EXPECT_FALSE(fingerprint_save_allowed(3, 2, false));
  // A refused SUBSCRIBE or an unsubscribe on this connection
  EXPECT_FALSE(fingerprint_save_allowed(3, 3, true));
}

}  // namespace esphome::mqtt::testing
