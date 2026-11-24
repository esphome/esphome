#include <gtest/gtest.h>

#include <random>
#include <string>

#include "esphome/components/mqtt/mqtt_topic_match.h"

namespace esphome::mqtt::testing {

// The previous recursive implementation, kept to check the loop matches it
static bool reference_match(const char *message, const char *subscription, bool is_normal, bool past_separator) {
  if (*message == '\0' && *subscription == '\0')
    return true;
  if (*message == '\0' || *subscription == '\0')
    return false;
  bool do_wildcards = is_normal || past_separator;
  if (*subscription == '+' && do_wildcards) {
    subscription++;
    while (*message != '\0' && *message != '/')
      message++;
    return reference_match(message, subscription, is_normal, true);
  }
  if (*subscription == '#' && do_wildcards)
    return true;
  if (*message != *subscription)
    return false;
  past_separator = past_separator || *subscription == '/';
  return reference_match(message + 1, subscription + 1, is_normal, past_separator);
}

static bool reference_match(const char *message, const char *subscription) {
  return reference_match(message, subscription, *message != '\0' && *message != '$', false);
}

TEST(MQTTTopicMatch, Exact) {
  EXPECT_TRUE(topic_match("a/b/c", "a/b/c"));
  EXPECT_FALSE(topic_match("a/b/c", "a/b"));
  EXPECT_FALSE(topic_match("a/b", "a/b/c"));
  EXPECT_FALSE(topic_match("a/b/c", "a/b/d"));
}

TEST(MQTTTopicMatch, SingleLevelWildcard) {
  EXPECT_TRUE(topic_match("a/b/c", "a/+/c"));
  EXPECT_TRUE(topic_match("a/b/c", "+/+/+"));
  EXPECT_TRUE(topic_match("a//c", "a/+/c"));
  EXPECT_FALSE(topic_match("a/b/c", "a/+"));
  EXPECT_FALSE(topic_match("a/b/c/d", "a/+/c"));
}

TEST(MQTTTopicMatch, MultiLevelWildcard) {
  EXPECT_TRUE(topic_match("a/b/c", "a/#"));
  EXPECT_TRUE(topic_match("a/b/c", "#"));
  EXPECT_TRUE(topic_match("a/b/c", "a/+/#"));
  EXPECT_FALSE(topic_match("b/c", "a/#"));
}

TEST(MQTTTopicMatch, DollarTopicsSkipFirstLevelWildcards) {
  EXPECT_FALSE(topic_match("$SYS/broker", "#"));
  EXPECT_FALSE(topic_match("$SYS/broker", "+/broker"));
  EXPECT_TRUE(topic_match("$SYS/broker", "$SYS/#"));
  EXPECT_TRUE(topic_match("$SYS/broker", "$SYS/+"));
}

TEST(MQTTTopicMatch, EdgeCases) {
  EXPECT_TRUE(topic_match("", ""));
  EXPECT_FALSE(topic_match("a", ""));
  // Kept from the previous code; the MQTT spec has "#" match the parent level and "+" an empty last level
  EXPECT_FALSE(topic_match("a/b", "a/b/#"));
  EXPECT_FALSE(topic_match("a/", "a/+"));
  EXPECT_TRUE(topic_match("a/$x", "a/+"));
}

// Recursion overflowed the ESP32 loop stack at about 250 characters
TEST(MQTTTopicMatch, LongTopic) {
  std::string topic(4096, 'x');
  topic[100] = '/';
  EXPECT_TRUE(topic_match(topic.c_str(), topic.c_str()));
  std::string other = topic;
  other.back() = 'y';
  EXPECT_FALSE(topic_match(topic.c_str(), other.c_str()));
}

TEST(MQTTTopicMatch, MatchesPreviousImplementation) {
  // Small alphabet so wildcards, separators and '$' meet often
  static constexpr char MESSAGE_CHARS[] = {'a', 'b', '/', '$'};
  static constexpr char SUBSCRIPTION_CHARS[] = {'a', 'b', '/', '$', '+', '#'};
  std::mt19937 rng(1234);  // NOLINT(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed) reproducible
  auto random_string = [&rng](const char *chars, size_t count) {
    std::string s(rng() % 8, ' ');
    for (char &c : s)
      c = chars[rng() % count];
    return s;
  };
  for (int i = 0; i < 20000; i++) {
    std::string message = random_string(MESSAGE_CHARS, sizeof(MESSAGE_CHARS));
    std::string subscription = random_string(SUBSCRIPTION_CHARS, sizeof(SUBSCRIPTION_CHARS));
    ASSERT_EQ(topic_match(message.c_str(), subscription.c_str()),
              reference_match(message.c_str(), subscription.c_str()))
        << "message='" << message << "' subscription='" << subscription << "'";
  }
}

}  // namespace esphome::mqtt::testing
