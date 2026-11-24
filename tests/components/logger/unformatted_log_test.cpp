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
    // The header always leaves room for the terminator, so the body starts inside the buffer
    for (uint16_t start = 0; start < size; start++) {
      for (const char *text : texts) {
        SCOPED_TRACE(::testing::Message() << "size " << size << " start " << start << " text '" << text << "'");
        char formatted[max_size], unformatted[max_size], flash[max_size];
        memset(formatted, '#', sizeof(formatted));
        memset(unformatted, '#', sizeof(unformatted));
        memset(flash, '#', sizeof(flash));
        LogBuffer a{formatted, size, start};
        LogBuffer b{unformatted, size, start};
        LogBuffer c{flash, size, start};
        format_into(a, "%s", text);
        b.write_unformatted_body(text, strlen(text));
        // The flash copy reads whole aligned words, so give it a zero padded copy
        alignas(4) char padded[128] = {};
        char *const in_flash = padded + (start & 3);
        memcpy(in_flash, text, strlen(text) + 1);
        c.write_unformatted_body_p(in_flash);
        ASSERT_EQ(a.pos, b.pos);
        ASSERT_EQ(a.pos, c.pos);
        // Up to and including the null terminator; vsnprintf() may leave bytes after it that nothing reads
        const size_t used = a.pos + 1;
        ASSERT_EQ(memcmp(formatted, unformatted, used), 0);
        ASSERT_EQ(memcmp(formatted, flash, used), 0);
      }
    }
  }
}

struct PlainLine {
  uint8_t level;
  std::string message;
};

class UnformattedLoggerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->previous_ = global_logger;
    this->logger_.pre_setup();
    this->logger_.set_log_level(ESPHOME_LOG_LEVEL_DEBUG);
    this->logger_.add_log_callback(this, capture);
  }
  void TearDown() override { global_logger = this->previous_; }

  static void capture(void *instance, uint8_t level, const char *tag, const char *message, size_t len) {
    auto *test = static_cast<UnformattedLoggerTest *>(instance);
    test->lines_.push_back({level, std::string(message, len)});
#ifdef USE_ESPHOME_TASK_LOG_BUFFER
    // As an on_message automation that restarts the device does
    if (test->power_down_from_listener_) {
      test->power_down_from_listener_ = false;
      test->logger_.on_powerdown();
    }
#endif
  }

#ifdef USE_HOST
  void log_from_worker(const char *text) {
    this->logger_.create_pthread_key();
    std::thread worker([text]() { esp_log_str_(ESPHOME_LOG_LEVEL_INFO, PLAIN_TAG, 7, text); });
    worker.join();
  }
#endif

  Logger *previous_{nullptr};
  Logger logger_{0};
  std::vector<PlainLine> lines_;
  bool power_down_from_listener_{false};
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

TEST_F(UnformattedLoggerTest, FormatExpressionIsEvaluatedOnce) {
  const char *const messages[] = {"first", "second"};
  int index = 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
  ESP_LOGW(PLAIN_TAG, messages[index++]);
#pragma GCC diagnostic pop
  EXPECT_EQ(index, 1);
  ASSERT_EQ(this->lines_.size(), 1u);
  EXPECT_NE(this->lines_[0].message.find("]: first"), std::string::npos) << this->lines_[0].message;
}

TEST_F(UnformattedLoggerTest, TemporaryFormatOutlivesTheCall) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
  ESP_LOGW(PLAIN_TAG, std::string(80, 'x').c_str());
#pragma GCC diagnostic pop
  ASSERT_EQ(this->lines_.size(), 1u);
  EXPECT_NE(this->lines_[0].message.find("]: " + std::string(80, 'x')), std::string::npos) << this->lines_[0].message;
}

TEST_F(UnformattedLoggerTest, LevelIsStillFiltered) {
  this->logger_.set_log_level(ESPHOME_LOG_LEVEL_WARN);
  ESP_LOGI(PLAIN_TAG, "hidden");
  ESP_LOGW(PLAIN_TAG, "shown");
  ASSERT_EQ(this->lines_.size(), 1u);
  EXPECT_EQ(this->lines_[0].level, ESPHOME_LOG_LEVEL_WARN);
}

#if defined(USE_HOST) && defined(USE_ESPHOME_TASK_LOG_BUFFER)
TEST_F(UnformattedLoggerTest, WorkerThreadLineGoesThroughTaskBuffer) {
  // loop() prints it, and on_powerdown() once a restart has stopped the loop
  for (auto drain : {&Logger::loop, &Logger::on_powerdown}) {
    this->lines_.clear();
    this->log_from_worker("from a worker");
    EXPECT_TRUE(this->lines_.empty());
    (this->logger_.*drain)();
    ASSERT_EQ(this->lines_.size(), 1u);
    EXPECT_NE(this->lines_[0].message.find("from a worker"), std::string::npos) << this->lines_[0].message;
  }
}

TEST_F(UnformattedLoggerTest, PowerdownFromAListenerDoesNotDeliverTheLineAgain) {
  this->power_down_from_listener_ = true;
  this->log_from_worker("restart now");
  this->logger_.loop();
  EXPECT_EQ(this->lines_.size(), 1u);
}
#endif  // USE_HOST && USE_ESPHOME_TASK_LOG_BUFFER

}  // namespace esphome::logger::testing
