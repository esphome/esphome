#include <gtest/gtest.h>
#include "esphome/components/logger/log_buffer.h"
#include "esphome/components/logger/logger.h"
#include <cstdarg>
#include <cstring>
#include <memory>
#include <string>

namespace esphome::logger::testing {

#ifdef USE_LOGGER_CRLF_LINE_ENDINGS
#define LINE_ENDING "\r\n"
#else
#define LINE_ENDING "\n"
#endif

#define LF "\n"
#define ANSI_RESET "\x1B[0m"
#define HEADER "\x1B[1;31m[E][TestTag:042]\x1B[1;31m[TestThread]\x1B[1;31m: "

static void reset_flash_format_call_flag_if_applicable() {
#ifdef USE_STORE_LOG_STR_IN_FLASH
  vsnprintf_P_called = false;
#endif
}

static bool was_flash_format_called_if_applicable() {
#ifdef USE_STORE_LOG_STR_IN_FLASH
  return vsnprintf_P_called;
#else
  return true;
#endif
}

template<typename FormatType>
void write_to_log_buffer(uint8_t level, const char *tag, int line, LogBuffer &buf, const char *thread_name,
                         FormatType format, ...) {
  va_list arg;
  va_start(arg, format);
  buf.write_header(level, tag, line, thread_name);
  buf.format_body(format, arg);
  va_end(arg);
  buf.terminate_with_newline();
}

static void write_fixed_test_message(LogBuffer &buf, const char *msg) {
#ifdef USE_STORE_LOG_STR_IN_FLASH
  write_to_log_buffer(1, "TestTag", 42, buf, "TestThread", (PGM_P) msg);
#else
  write_to_log_buffer(1, "TestTag", 42, buf, "TestThread", msg);
#endif
}

LogBuffer create_test_log_buffer(uint16_t internal_size) {
  // Intentionally leaked: the buffer must outlive this function and is only used for the duration of the test.
  auto tx_buffer = std::make_unique<char[]>(internal_size + 1);  // +1 for null terminator
  LogBuffer buf{tx_buffer.release(), internal_size};
  return buf;
}

static constexpr const char *TEST_GENERATION_PARAMS[] = {"a" LINE_ENDING "aa", "b\r\nbb", "ccc"};

class LogBufferTest : public ::testing::TestWithParam<const char *> {};

TEST_P(LogBufferTest, TestDifferentStrings) {
  // given
  reset_flash_format_call_flag_if_applicable();
  auto buf = create_test_log_buffer(1000);
  const auto *msg = GetParam();

  // when
  write_fixed_test_message(buf, msg);

  // then
  auto expected_msg = std::string(HEADER) + std::string(msg) + std::string(ANSI_RESET LINE_ENDING);
  EXPECT_EQ(buf.pos, expected_msg.length());
  EXPECT_EQ(std::string(buf.data, buf.pos), expected_msg);
  EXPECT_TRUE(was_flash_format_called_if_applicable());
}

TEST(LogBufferTest, HeaderDoesNotFit) {
  // given
  reset_flash_format_call_flag_if_applicable();
  auto buf = create_test_log_buffer(50);
  const auto *msg = "Test 1234" LINE_ENDING "haha";

  // when
  write_fixed_test_message(buf, msg);

  // then
  auto expected_msg = std::string(msg) + std::string(ANSI_RESET) + std::string(LINE_ENDING);
  EXPECT_EQ(buf.pos, expected_msg.length());
  EXPECT_EQ(std::string(buf.data, buf.pos), expected_msg);
  EXPECT_TRUE(was_flash_format_called_if_applicable());
}

TEST(LogBufferTest, ManyNewlines) {
  // given
  reset_flash_format_call_flag_if_applicable();
  // Buffer large enough to hold the fully CRLF-expanded message with no truncation.
  auto buf = create_test_log_buffer(128 + 30);
#define NINE_LFS LF LF LF LF LF LF LF LF LF
#define NINE_LINE_ENDINGS \
  LINE_ENDING LINE_ENDING LINE_ENDING LINE_ENDING LINE_ENDING LINE_ENDING LINE_ENDING LINE_ENDING LINE_ENDING
  // The source message always contains raw "\n" - CRLF expansion (if enabled) happens inside format_body().
  const auto *msg = "asdfasdfasdfasdfasdfasdfasdfasdfa1dfasdfasdfas2fasddfasdfas3fasd" NINE_LFS "haha";

  // when
  write_fixed_test_message(buf, msg);

  // then
  auto expected_msg =
      std::string(HEADER "asdfasdfasdfasdfasdfasdfasdfasdfa1dfasdfasdfas2fasddfasdfas3fasd" NINE_LINE_ENDINGS
                         "haha\x1B[0m" LINE_ENDING);
  EXPECT_EQ(buf.pos, expected_msg.length());
  EXPECT_EQ(std::string(buf.data, buf.pos), expected_msg);
  EXPECT_TRUE(was_flash_format_called_if_applicable());
}

//"string with " LF " and " LF "ending with " LF

// Regression test: an undersized buffer with many embedded newlines must not overflow past `size`,
// even though CRLF expansion needs extra bytes beyond what vsnprintf wrote.
TEST(LogBufferTest, ManyNewlinesTruncatedWhenBufferTooSmall) {
  reset_flash_format_call_flag_if_applicable();
  auto buf = create_test_log_buffer(128 + 5);  // deliberately too small to fit all 9 expanded newlines
  const auto *msg = "asdfasdfasdfasdfasdfasdfasdfasdfa1dfasdfasdfas2fasddfasdfas3fasd" NINE_LFS "haha";

  write_fixed_test_message(buf, msg);

  EXPECT_LE(buf.pos, buf.size);
  EXPECT_TRUE(was_flash_format_called_if_applicable());
}

// Regression test: when the header doesn't fit (pos stays 0) and the message body consists solely of
// newlines longer than the buffer, the trailing-newline-discard loop must not read before `data[0]`.
TEST(LogBufferTest, AllNewlinesWithoutHeaderDoesNotUnderflow) {
  reset_flash_format_call_flag_if_applicable();
  auto buf = create_test_log_buffer(10);                  // too small for the header -> pos stays 0
  const auto *msg = LF LF LF LF LF LF LF LF LF LF LF LF;  // more LFs than the buffer can hold

  write_fixed_test_message(buf, msg);

  EXPECT_LE(buf.pos, buf.size);
  EXPECT_TRUE(was_flash_format_called_if_applicable());
}

INSTANTIATE_TEST_SUITE_P(MyTest, LogBufferTest, ::testing::ValuesIn(TEST_GENERATION_PARAMS));

}  // namespace esphome::logger::testing
