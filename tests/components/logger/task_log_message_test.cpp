#include <gtest/gtest.h>
#include <cstring>

#include "esphome/core/defines.h"

// The cap and message header are compiled only for platforms with a task log buffer
#define USE_LIBRETINY
#ifndef USE_ESPHOME_TASK_LOG_BUFFER
#define USE_ESPHOME_TASK_LOG_BUFFER
#endif
#include "esphome/components/logger/task_log_message.h"
#undef USE_LIBRETINY

namespace esphome::logger::testing {

TEST(TaskLogMessageTest, CapLengthStopsAt255) {
  EXPECT_EQ(task_log_cap_length(0), 0);
  EXPECT_EQ(task_log_cap_length(255), 255);
  EXPECT_EQ(task_log_cap_length(256), 255);
  EXPECT_EQ(task_log_cap_length(1000), 255);
}

TEST(TaskLogMessageTest, HeaderCopiesFieldsAndThreadName) {
  TaskLogMessage msg{};
  fill_task_log_header(&msg, 3, "tag", 42, "worker");
  EXPECT_EQ(msg.level, 3);
  EXPECT_STREQ(msg.tag, "tag");
  EXPECT_EQ(msg.line, 42);
  EXPECT_STREQ(msg.thread_name, "worker");
}

TEST(TaskLogMessageTest, HeaderNullThreadNameIsEmpty) {
  TaskLogMessage msg{};
  std::memset(msg.thread_name, 'x', sizeof(msg.thread_name));
  fill_task_log_header(&msg, 1, "tag", 1, nullptr);
  EXPECT_STREQ(msg.thread_name, "");
}

TEST(TaskLogMessageTest, HeaderTruncatesLongThreadName) {
  TaskLogMessage msg{};
  fill_task_log_header(&msg, 1, "tag", 1, "a_thread_name_longer_than_sixteen");
  EXPECT_EQ(std::strlen(msg.thread_name), sizeof(msg.thread_name) - 1);
  EXPECT_EQ(std::strncmp(msg.thread_name, "a_thread_name_l", sizeof(msg.thread_name) - 1), 0);
}

TEST(TaskLogMessageTest, TrimTrailingNewlines) {
  EXPECT_EQ(trim_trailing_newlines("", 0), 0);
  EXPECT_EQ(trim_trailing_newlines("abc", 3), 3);
  EXPECT_EQ(trim_trailing_newlines("abc\n\n", 5), 3);
  EXPECT_EQ(trim_trailing_newlines("\n\n\n", 3), 0);
  EXPECT_EQ(trim_trailing_newlines("a\nb", 3), 3);
}

}  // namespace esphome::logger::testing
