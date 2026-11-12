#include <gtest/gtest.h>

#include <string>

#include "esphome/components/mqtt/mqtt_topic_match.h"

namespace esphome::mqtt::testing {

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
  // MQTT wildcard levels may match zero characters / zero trailing levels.
  EXPECT_TRUE(topic_match("a/b", "a/b/#"));
  EXPECT_TRUE(topic_match("a", "a/#"));
  EXPECT_TRUE(topic_match("a/", "a/+"));
  EXPECT_TRUE(topic_match("a//", "a/+/+"));
  EXPECT_FALSE(topic_match("a", "a/+"));
  EXPECT_FALSE(topic_match("a/b", "a/+/c"));
  EXPECT_TRUE(topic_match("a/$x", "a/+"));
}

// Recursion overflowed the ESP32 loop stack at about 250 characters.
TEST(MQTTTopicMatch, LongTopic) {
  std::string topic(4096, 'x');
  topic[100] = '/';
  EXPECT_TRUE(topic_match(topic.c_str(), topic.c_str()));
  std::string other = topic;
  other.back() = 'y';
  EXPECT_FALSE(topic_match(topic.c_str(), other.c_str()));
}

}  // namespace esphome::mqtt::testing
