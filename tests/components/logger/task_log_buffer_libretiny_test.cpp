#ifdef USE_HOST
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <random>
#include <string>
#include <utility>

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

// Not a multiple of 4; task_log_buffer_libretiny_768_test.cpp reuses this file with an aligned size
#ifndef TASK_LOG_TEST_NS
#define TASK_LOG_TEST_NS libretiny_task_log_766
#define TASK_LOG_TEST_SUITE LibreTinyTaskLogBuffer766
#define TASK_LOG_TEST_SIZE static_cast<size_t>(766)
#endif

// Compile the LibreTiny buffer against the FreeRTOS doubles in __libretiny_sdk, in a namespace per size
#define USE_LIBRETINY
#ifndef USE_ESPHOME_TASK_LOG_BUFFER
#define USE_ESPHOME_TASK_LOG_BUFFER
#endif
#undef ESPHOME_TASK_LOG_BUFFER_SIZE
#define ESPHOME_TASK_LOG_BUFFER_SIZE TASK_LOG_TEST_SIZE
namespace TASK_LOG_TEST_NS {
// Not global, where speaker_source's FreeRTOS double defines it in the same build
inline constexpr int pdTRUE = 1;
// NOLINTNEXTLINE(bugprone-suspicious-include): exercise production code with the host FreeRTOS double.
#include "esphome/components/logger/task_log_buffer_libretiny.cpp"
}  // namespace TASK_LOG_TEST_NS
#undef USE_LIBRETINY

namespace {

using TaskLogBuffer = TASK_LOG_TEST_NS::esphome::logger::TaskLogBuffer;

const char *const TAGS[] = {"a", "bb", "ccc"};

bool send_format(TaskLogBuffer &buffer, const char *tag, const char *format, ...) {
  va_list args;
  va_start(args, format);
  bool sent = buffer.send_message_thread_safe(3, tag, 1, nullptr, format, args);
  va_end(args);
  return sent;
}

class TASK_LOG_TEST_SUITE : public ::testing::Test {
 protected:
  // Records the trimmed text expected back when the buffer accepts it
  bool send_(const char *tag, const std::string &text) {
    if (!send_format(this->buffer_, tag, "%s", text.c_str()))
      return false;
    std::string trimmed = text;
    while (!trimmed.empty() && trimmed.back() == '\n')
      trimmed.pop_back();
    this->expected_.emplace_back(tag, trimmed);
    return true;
  }
  // Stops one past what was sent, so a broken ring cannot loop forever
  void drain_(size_t count = SIZE_MAX) {
    TaskLogBuffer::LogMessage *msg;
    uint16_t length;
    for (count = std::min(count, this->expected_.size() + 1);
         count > 0 && this->buffer_.borrow_message_main_loop(msg, length); count--) {
      if (reinterpret_cast<uintptr_t>(msg) % alignof(TaskLogBuffer::LogMessage) != 0)
        this->misaligned_++;
      if (this->expected_.empty() || this->expected_.front().first != msg->tag ||
          this->expected_.front().second != std::string(msg->text_data(), length) || msg->text_data()[length] != '\0') {
        this->mismatches_++;
      } else {
        this->expected_.pop_front();
      }
      this->buffer_.release_message_main_loop();
    }
  }

  TaskLogBuffer buffer_;
  std::deque<std::pair<const char *, std::string>> expected_;
  int misaligned_{0};
  int mismatches_{0};
};

TEST_F(TASK_LOG_TEST_SUITE, HeadersStayAligned) {
  for (size_t length = 1; length <= 40; length++) {
    ASSERT_TRUE(this->send_(TAGS[0], std::string(length, 'x')));
    if (length % 4 == 0)
      this->drain_();
  }
  this->drain_();
  EXPECT_EQ(this->misaligned_, 0);
  EXPECT_EQ(this->mismatches_, 0);
}

TEST_F(TASK_LOG_TEST_SUITE, TrimsTrailingNewlinesAndKeepsOrder) {
  ASSERT_TRUE(this->send_(TAGS[0], "one"));
  ASSERT_TRUE(this->send_(TAGS[1], "two\n"));
  ASSERT_TRUE(this->send_(TAGS[2], "three\n\n"));
  ASSERT_TRUE(this->send_(TAGS[0], "four\nlines"));
  this->drain_();
  EXPECT_EQ(this->mismatches_, 0);
  EXPECT_TRUE(this->expected_.empty());
  EXPECT_FALSE(this->buffer_.has_messages());
}

TEST_F(TASK_LOG_TEST_SUITE, EmptyBufferAcceptsEveryLengthAcrossWraps) {
  // Every write position comes up, including a message ending less than a header before the end
  for (int round = 0; round < 8; round++) {
    for (size_t length = 1; length <= 250; length++) {
      ASSERT_TRUE(this->send_(TAGS[length % 3], std::string(length, 'a' + length % 26) + (round % 2 ? "\n" : "")))
          << "round " << round << " length " << length;
      this->drain_();
    }
  }
  EXPECT_EQ(this->misaligned_, 0);
  EXPECT_EQ(this->mismatches_, 0);
}

TEST_F(TASK_LOG_TEST_SUITE, RandomizedTrafficMatchesWhatWasSent) {
  std::mt19937 rng(1);
  for (int op = 0; op < 200000; op++) {
    if (rng() % 3 != 0) {
      std::string text(1 + rng() % 120, 'x');
      for (auto &c : text)
        c = 'a' + rng() % 26;
      if (rng() % 2)
        text += "\n";
      if (rng() % 5 == 0)
        text += "\n\n";
      this->send_(TAGS[rng() % 3], text);
    } else {
      this->drain_(rng() % 4);
    }
  }
  this->drain_();
  EXPECT_EQ(this->misaligned_, 0);
  EXPECT_EQ(this->mismatches_, 0);
  EXPECT_TRUE(this->expected_.empty());
  EXPECT_TRUE(this->send_(TAGS[0], "still accepted"));
}

}  // namespace
#endif  // USE_HOST
