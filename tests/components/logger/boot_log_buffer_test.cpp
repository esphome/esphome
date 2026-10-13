#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "esphome/components/logger/logger.h"
#include "esphome/core/log.h"

namespace esphome::logger::testing {

ESPHOME_LOG_TAG(BOOT_TAG, "boot.test");

struct Captured {
  uint8_t level;
  const char *tag;
  std::string message;
};

// Collects what a listener such as the API connection would receive.
static void capture(void *instance, uint8_t level, const char *tag, const char *message, size_t len) {
  static_cast<std::vector<Captured> *>(instance)->push_back({level, tag, std::string(message, len)});
}

class BootLogBufferTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->previous_ = global_logger;
    this->logger_ = new Logger(0);  // NOLINT: freed in TearDown, Logger is not movable
    this->logger_->pre_setup();
    this->logger_->set_log_level(ESPHOME_LOG_LEVEL_DEBUG);
  }
  void TearDown() override {
    delete this->logger_;
    global_logger = this->previous_;
  }
  void listen() { this->logger_->add_log_callback(&this->lines_, capture); }
  std::vector<Captured> replayed() const {
    std::vector<Captured> out;
    for (const auto &line : this->lines_) {
      if (line.message.find("@") != std::string::npos && line.message.find("ms ") != std::string::npos)
        out.push_back(line);
    }
    return out;
  }

  Logger *logger_{nullptr};
  Logger *previous_{nullptr};
  std::vector<Captured> lines_;
};

TEST_F(BootLogBufferTest, ReplaysWarningsAndErrorsWithTheirTagAndLevel) {
  ESP_LOGW(BOOT_TAG, "early warning %d", 1);
  ESP_LOGE(BOOT_TAG, "early error");
  ESP_LOGI(BOOT_TAG, "info is not kept");

  this->listen();
  this->logger_->replay_boot_logs();

  const auto lines = this->replayed();
  ASSERT_EQ(lines.size(), 2u);
  EXPECT_EQ(lines[0].level, ESPHOME_LOG_LEVEL_WARN);
  EXPECT_EQ(lines[0].tag, BOOT_TAG);
  EXPECT_NE(lines[0].message.find("[W][boot.test:"), std::string::npos) << lines[0].message;
  EXPECT_NE(lines[0].message.find("early warning 1"), std::string::npos) << lines[0].message;
  EXPECT_EQ(lines[1].level, ESPHOME_LOG_LEVEL_ERROR);
  EXPECT_NE(lines[1].message.find("early error"), std::string::npos) << lines[1].message;
  // One header per line: the stored header is not formatted a second time
  EXPECT_EQ(lines[1].message.find("[E]"), lines[1].message.rfind("[E]")) << lines[1].message;
}

TEST_F(BootLogBufferTest, ReplayOutputIsNotCapturedAgain) {
  ESP_LOGW(BOOT_TAG, "once");
  this->listen();
  this->logger_->replay_boot_logs();
  ESP_LOGW(BOOT_TAG, "after replay");
  this->lines_.clear();
  this->logger_->replay_boot_logs();
  const auto lines = this->replayed();
  ASSERT_EQ(lines.size(), 1u);
  EXPECT_NE(lines[0].message.find("once"), std::string::npos) << lines[0].message;
}

TEST_F(BootLogBufferTest, LinesThatDoNotFitAreCounted) {
  for (int i = 0; i < 40; i++) {
    ESP_LOGW(BOOT_TAG, "warning number %d padded to make the record longer", i);
  }
  this->listen();
  this->logger_->replay_boot_logs();
  const auto lines = this->replayed();
  EXPECT_GT(lines.size(), 0u);
  EXPECT_LT(lines.size(), 40u);
  // The replay summary reports the dropped lines
  bool summary = false;
  for (const auto &line : this->lines_) {
    if (line.message.find("lines dropped") != std::string::npos &&
        line.message.find(" 0 lines dropped") == std::string::npos)
      summary = true;
  }
  EXPECT_TRUE(summary);
}

TEST_F(BootLogBufferTest, LongestLineIsReplayedWithoutLosingItsTail) {
  // A line that fills the transmit buffer must still fit once the replay adds its stamp
  std::string text(ESPHOME_LOGGER_TX_BUFFER_SIZE, 'x');
  ESP_LOGW(BOOT_TAG, "%s", text.c_str());
  this->listen();
  this->logger_->replay_boot_logs();

  const auto lines = this->replayed();
  ASSERT_EQ(lines.size(), 1u);
  EXPECT_LE(lines[0].message.size(), static_cast<size_t>(ESPHOME_LOGGER_TX_BUFFER_SIZE));
  // The reset code at the end shows the stored text was not cut short by the prefix
  EXPECT_EQ(lines[0].message.substr(lines[0].message.size() - 4), "\033[0m") << lines[0].message.size();
  EXPECT_NE(lines[0].message.find("xxxxxxxxxx\033[0m"), std::string::npos);
}

}  // namespace esphome::logger::testing
