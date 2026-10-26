#include <gtest/gtest.h>

#include <algorithm>
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
    this->logger_.pre_setup();
    this->logger_.set_log_level(ESPHOME_LOG_LEVEL_DEBUG);
  }
  void TearDown() override { global_logger = this->previous_; }

  // Attach a listener after the early lines, replay, and return the replayed lines (they carry the boot tag)
  std::vector<Captured> replay_() {
    this->logger_.add_log_callback(&this->lines_, capture);
    this->logger_.replay_boot_logs();
    std::vector<Captured> out;
    for (const auto &line : this->lines_) {
      if (line.tag == BOOT_TAG)
        out.push_back(line);
    }
    return out;
  }
  bool summary_warned_() const {
    return std::any_of(this->lines_.begin(), this->lines_.end(),
                       [](const Captured &l) { return l.tag != BOOT_TAG && l.level == ESPHOME_LOG_LEVEL_WARN; });
  }

  Logger *previous_{nullptr};
  Logger logger_{0};
  std::vector<Captured> lines_;
};

TEST_F(BootLogBufferTest, ReplaysWarningsAndErrorsWithTheirTagAndLevel) {
  ESP_LOGW(BOOT_TAG, "early warning %d", 1);
  ESP_LOGE(BOOT_TAG, "early error");
  ESP_LOGI(BOOT_TAG, "info is not kept");

  const auto lines = this->replay_();
  ASSERT_EQ(lines.size(), 2u);
  EXPECT_EQ(lines[0].level, ESPHOME_LOG_LEVEL_WARN);
  EXPECT_NE(lines[0].message.find("[W][boot.test:"), std::string::npos) << lines[0].message;
  EXPECT_NE(lines[0].message.find("early warning 1"), std::string::npos) << lines[0].message;
  EXPECT_EQ(lines[1].level, ESPHOME_LOG_LEVEL_ERROR);
  EXPECT_NE(lines[1].message.find("early error"), std::string::npos) << lines[1].message;
  // One header per line: the stored header is not formatted a second time
  EXPECT_EQ(lines[1].message.find("[E]"), lines[1].message.rfind("[E]")) << lines[1].message;
  EXPECT_FALSE(this->summary_warned_());
}

TEST_F(BootLogBufferTest, ReplayOutputIsNotCapturedAgain) {
  ESP_LOGW(BOOT_TAG, "once");
  this->replay_();
  ESP_LOGW(BOOT_TAG, "after replay");
  this->lines_.clear();
  this->logger_.replay_boot_logs();
  size_t replayed = 0;
  for (const auto &line : this->lines_) {
    if (line.tag == BOOT_TAG) {
      replayed++;
      EXPECT_NE(line.message.find("once"), std::string::npos) << line.message;
    }
  }
  EXPECT_EQ(replayed, 1u);
}

TEST_F(BootLogBufferTest, LinesThatDoNotFitAreCountedAndWarned) {
  for (int i = 0; i < 40; i++) {
    ESP_LOGW(BOOT_TAG, "warning number %d padded to make the record longer", i);
  }
  const auto lines = this->replay_();
  EXPECT_GT(lines.size(), 0u);
  EXPECT_LT(lines.size(), 40u);
  EXPECT_TRUE(this->summary_warned_());
}

TEST_F(BootLogBufferTest, LongestLineIsReplayedWithoutLosingItsTail) {
  // A line that fills the transmit buffer must still fit once the replay adds its stamp
  std::string text(ESPHOME_LOGGER_TX_BUFFER_SIZE, 'x');
  ESP_LOGW(BOOT_TAG, "%s", text.c_str());
  const auto lines = this->replay_();
  ASSERT_EQ(lines.size(), 1u);
  EXPECT_LE(lines[0].message.size(), static_cast<size_t>(ESPHOME_LOGGER_TX_BUFFER_SIZE));
  // Text runs right up to the reset code, so nothing was cut by the replay header
  EXPECT_EQ(lines[0].message.substr(lines[0].message.size() - 5), "x\033[0m") << lines[0].message.size();
}

}  // namespace esphome::logger::testing
