#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "esphome/components/logger/logger.h"
#include "esphome/core/log.h"

namespace esphome::logger::testing {

ESPHOME_LOG_TAG(TRIGGER_TAG, "trigger.test");

struct MessageEvent {
  uint8_t level;
  std::string tag;
  std::string message;
};

// What an on_message automation is given each time it fires
struct Recorder {
  std::vector<MessageEvent> events;
  bool log_once{false};  // the action logs a line of its own, once
  void record(uint8_t level, const char *tag, const char *message) {
    this->events.push_back({level, std::string(tag), std::string(message)});
    if (this->log_once) {
      this->log_once = false;
      ESP_LOGW(TRIGGER_TAG, "nested");
    }
  }
};

// An on_message automation at LEVEL whose actions record into `rec`
template<uint8_t LEVEL> struct OnMessage {
  OnMessage(Logger *logger, Recorder *rec) {
    recorder = rec;
    logger->add_log_callback(nullptr, on_message_callback<LEVEL, record>);
  }
  static void record(const uint8_t &level, const char *const &tag, const char *const &message) {
    recorder->record(level, tag, message);
  }
  static inline Recorder *recorder{nullptr};
};

class LoggerMessageTriggerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->previous_ = global_logger;
    this->logger_.pre_setup();
    this->logger_.set_log_level(ESPHOME_LOG_LEVEL_DEBUG);
  }
  void TearDown() override { global_logger = this->previous_; }

  Logger *previous_{nullptr};
  Logger logger_{0};
};

TEST_F(LoggerMessageTriggerTest, FiresAtTheLevelAndMoreSevere) {
  Recorder rec;
  OnMessage<ESPHOME_LOG_LEVEL_WARN> on_message(&this->logger_, &rec);
  ESP_LOGD(TRIGGER_TAG, "debug");
  ESP_LOGI(TRIGGER_TAG, "info");
  ESP_LOGW(TRIGGER_TAG, "warn");
  ESP_LOGE(TRIGGER_TAG, "error");
  ASSERT_EQ(rec.events.size(), 2u);
  EXPECT_EQ(rec.events[0].level, ESPHOME_LOG_LEVEL_WARN);
  EXPECT_EQ(rec.events[1].level, ESPHOME_LOG_LEVEL_ERROR);
}

TEST_F(LoggerMessageTriggerTest, PassesLevelTagAndFormattedMessage) {
  Recorder rec;
  OnMessage<ESPHOME_LOG_LEVEL_DEBUG> on_message(&this->logger_, &rec);
  ESP_LOGI(TRIGGER_TAG, "value %d", 42);
  ASSERT_EQ(rec.events.size(), 1u);
  EXPECT_EQ(rec.events[0].level, ESPHOME_LOG_LEVEL_INFO);
  EXPECT_EQ(rec.events[0].tag, "trigger.test");
  EXPECT_NE(rec.events[0].message.find("[trigger.test"), std::string::npos) << rec.events[0].message;
  EXPECT_NE(rec.events[0].message.find("]: value 42"), std::string::npos) << rec.events[0].message;
}

TEST_F(LoggerMessageTriggerTest, EachTriggerFiltersOnItsOwnLevel) {
  Recorder errors, all;
  OnMessage<ESPHOME_LOG_LEVEL_ERROR> on_error(&this->logger_, &errors);
  OnMessage<ESPHOME_LOG_LEVEL_DEBUG> on_debug(&this->logger_, &all);
  ESP_LOGD(TRIGGER_TAG, "debug");
  ESP_LOGW(TRIGGER_TAG, "warn");
  ESP_LOGE(TRIGGER_TAG, "error");
  ASSERT_EQ(errors.events.size(), 1u);
  EXPECT_EQ(errors.events[0].level, ESPHOME_LOG_LEVEL_ERROR);
  ASSERT_EQ(all.events.size(), 3u);
  EXPECT_EQ(all.events[0].level, ESPHOME_LOG_LEVEL_DEBUG);
  EXPECT_EQ(all.events[1].level, ESPHOME_LOG_LEVEL_WARN);
  EXPECT_EQ(all.events[2].level, ESPHOME_LOG_LEVEL_ERROR);
}

TEST_F(LoggerMessageTriggerTest, LinesBelowTheLoggerLevelNeverReachTheTrigger) {
  this->logger_.set_log_level(ESPHOME_LOG_LEVEL_WARN);
  Recorder rec;
  OnMessage<ESPHOME_LOG_LEVEL_DEBUG> on_message(&this->logger_, &rec);
  ESP_LOGI(TRIGGER_TAG, "hidden");
  ESP_LOGW(TRIGGER_TAG, "shown");
  ASSERT_EQ(rec.events.size(), 1u);
  EXPECT_EQ(rec.events[0].level, ESPHOME_LOG_LEVEL_WARN);
}

TEST_F(LoggerMessageTriggerTest, LineLoggedFromTheActionNeverReachesIt) {
  Recorder rec;
  rec.log_once = true;
  OnMessage<ESPHOME_LOG_LEVEL_WARN> on_message(&this->logger_, &rec);
  ESP_LOGW(TRIGGER_TAG, "outer");
  ASSERT_EQ(rec.events.size(), 1u);
  EXPECT_NE(rec.events[0].message.find("]: outer"), std::string::npos) << rec.events[0].message;
  EXPECT_FALSE(rec.log_once);
}

}  // namespace esphome::logger::testing
