#include <gtest/gtest.h>

#include <cstdarg>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "esphome/components/logger/log_buffer.h"
#include "esphome/components/logger/logger.h"
#include "esphome/core/log.h"

namespace esphome::logger::testing {

ESPHOME_LOG_TAG(PLAIN_TAG, "plain.test");

static void format_into(LogBuffer &buf, const char *format, ...) {
  va_list args;
  va_start(args, format);
  buf.format_body(format, args);
  va_end(args);
}

TEST(UnformattedLogTest, BodyMatchesFormattedBodyAtEverySize) {
  const std::string long_text(100, 'y');
  const char *texts[] = {"", "short", "ends with newlines\n\n", "\n", "a\nb", long_text.c_str()};
  constexpr uint16_t max_size = 48;
  for (uint16_t size = 1; size <= max_size; size++) {
    for (uint16_t start = 0; start <= size; start++) {
      for (const char *text : texts) {
        char formatted[max_size], unformatted[max_size];
        memset(formatted, '#', sizeof(formatted));
        memset(unformatted, '#', sizeof(unformatted));
        LogBuffer a{formatted, size, start};
        LogBuffer b{unformatted, size, start};
        format_into(a, "%s", text);
        b.write_unformatted_body(text, strlen(text));
        ASSERT_EQ(a.pos, b.pos) << "size " << size << " start " << start << " text '" << text << "'";
        // Up to and including the null terminator; vsnprintf() may leave bytes after it that nothing reads
        const size_t used = (a.pos < size) ? a.pos + 1 : size;
        ASSERT_EQ(memcmp(formatted, unformatted, used), 0)
            << "size " << size << " start " << start << " text '" << text << "'";
      }
    }
  }
}

struct PlainLine {
  uint8_t level;
  std::string message;
};

static void capture_plain(void *instance, uint8_t level, const char *tag, const char *message, size_t len) {
  static_cast<std::vector<PlainLine> *>(instance)->push_back({level, std::string(message, len)});
}

class UnformattedLoggerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->previous_ = global_logger;
    this->logger_.pre_setup();
    this->logger_.set_log_level(ESPHOME_LOG_LEVEL_DEBUG);
    this->logger_.add_log_callback(&this->lines_, capture_plain);
  }
  void TearDown() override { global_logger = this->previous_; }

  Logger *previous_{nullptr};
  Logger logger_{0};
  std::vector<PlainLine> lines_;
};

TEST_F(UnformattedLoggerTest, LineWithoutArgumentsMatchesFormattedLine) {
  esp_log_str_(ESPHOME_LOG_LEVEL_INFO, PLAIN_TAG, 7, "no arguments here\n");
  esp_log_printf_(ESPHOME_LOG_LEVEL_INFO, PLAIN_TAG, 7, "%s", "no arguments here\n");
  ASSERT_EQ(this->lines_.size(), 2u);
  EXPECT_EQ(this->lines_[0].level, ESPHOME_LOG_LEVEL_INFO);
  EXPECT_EQ(this->lines_[0].message, this->lines_[1].message);
  EXPECT_NE(this->lines_[0].message.find("[I][plain.test:007]: no arguments here\033[0m"), std::string::npos)
      << this->lines_[0].message;
}

TEST_F(UnformattedLoggerTest, PercentSignIsStillFormatted) {
  ESP_LOGW(PLAIN_TAG, "100%% done");
  ASSERT_EQ(this->lines_.size(), 1u);
  EXPECT_NE(this->lines_[0].message.find("]: 100% done"), std::string::npos) << this->lines_[0].message;
}

TEST_F(UnformattedLoggerTest, LevelIsStillFiltered) {
  this->logger_.set_log_level(ESPHOME_LOG_LEVEL_WARN);
  ESP_LOGI(PLAIN_TAG, "hidden");
  ESP_LOGW(PLAIN_TAG, "shown");
  ASSERT_EQ(this->lines_.size(), 1u);
  EXPECT_EQ(this->lines_[0].level, ESPHOME_LOG_LEVEL_WARN);
}

#ifdef USE_HOST
TEST_F(UnformattedLoggerTest, WorkerThreadLineGoesThroughTaskBuffer) {
  this->logger_.create_pthread_key();
  std::thread worker([]() { esp_log_str_(ESPHOME_LOG_LEVEL_INFO, PLAIN_TAG, 7, "from a worker"); });
  worker.join();
  EXPECT_TRUE(this->lines_.empty());
#ifdef USE_ESPHOME_TASK_LOG_BUFFER
  this->logger_.loop();
  ASSERT_EQ(this->lines_.size(), 1u);
  EXPECT_NE(this->lines_[0].message.find("from a worker"), std::string::npos) << this->lines_[0].message;
#endif
}
#endif  // USE_HOST

}  // namespace esphome::logger::testing
