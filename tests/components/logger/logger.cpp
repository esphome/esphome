#include <gtest/gtest.h>
#include "esphome/components/logger/logger.h"
#include <cstdarg>
#include <string>

namespace esphome::logger::testing {

void start_stdout_capture() { ::testing::internal::CaptureStdout(); }

std::string stop_stdout_capture() { return ::testing::internal::GetCapturedStdout(); }

template<typename FormatType>
void test_esp_log_printf(uint8_t level, const char *tag, int line, Logger &logger, const char *thread_name,
                         FormatType format, ...) {
  va_list arg;
  va_start(arg, format);
  logger.log_vprintf_(level, tag, line, format, arg);
  va_end(arg);
}

#ifdef USE_LOGGER_CRLF_LINE_ENDINGS

TEST(LoggerTest, LoggerTestWithCrlf) {
  auto *log = new logger::Logger(115200);  // NOLINT
  start_stdout_capture();
  test_esp_log_printf(1, "TestTag", 42, *log, "TestThread", "Test %d\nhaha", 1234);
  const std::string captured_output = stop_stdout_capture();
  EXPECT_NE(captured_output.find("[E][TestTag:042]"), std::string::npos);
  EXPECT_NE(captured_output.find("Test 1234\r\nhaha"), std::string::npos);
}
#ifdef USE_STORE_LOG_STR_IN_FLASH
TEST(LoggerTest, LoggerTestWithCrlfInFlash) {
  vsnprintf_P_called = false;
  auto *log = new logger::Logger(115200);  // NOLINT
  start_stdout_capture();
  test_esp_log_printf(1, "TestTag2", 42, *log, "TestThread2", (__FlashStringHelper *) "Test2 %d\nhaha", 1234);
  const std::string captured_flash_output = stop_stdout_capture();
  EXPECT_NE(captured_flash_output.find("[E][TestTag2:042]"), std::string::npos);
  EXPECT_NE(captured_flash_output.find("Test2 1234\r\nhaha"), std::string::npos);
  EXPECT_TRUE(vsnprintf_P_called);
}
#endif
#else
TEST(LogBufferTest, LoggerTestWithLf) {
  auto *log = new logger::Logger(115200);  // NOLINT
  start_stdout_capture();
  test_esp_log_printf(1, "TestTag", 42, *log, "TestThread", "Test %d\nhaha", 1234);
  const std::string captured_output = stop_stdout_capture();
  EXPECT_NE(captured_output.find("[E][TestTag:042]"), std::string::npos);
  EXPECT_NE(captured_output.find("Test 1234\nhaha"), std::string::npos);
}
#ifdef USE_STORE_LOG_STR_IN_FLASH
TEST(LoggerTest, LoggerTestWithLfInFlash) {
  vsnprintf_P_called = false;
  auto *log = new logger::Logger(115200);  // NOLINT
  start_stdout_capture();
  test_esp_log_printf(1, "TestTag2", 42, *log, "TestThread2", (__FlashStringHelper *) "Test2 %d\nhaha", 1234);
  const std::string captured_flash_output = stop_stdout_capture();
  EXPECT_NE(captured_flash_output.find("[E][TestTag2:042]"), std::string::npos);
  EXPECT_NE(captured_flash_output.find("Test2 1234\nhaha"), std::string::npos);
  EXPECT_TRUE(vsnprintf_P_called);
}
#endif
#endif

}  // namespace esphome::logger::testing
