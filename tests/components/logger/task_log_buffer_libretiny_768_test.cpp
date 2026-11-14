// The same tests with a buffer size that needs no rounding
#define TASK_LOG_TEST_NS libretiny_task_log_768
#define TASK_LOG_TEST_SUITE LibreTinyTaskLogBuffer768
#define TASK_LOG_TEST_SIZE static_cast<size_t>(768)
// NOLINTNEXTLINE(bugprone-suspicious-include): reuse the tests with another buffer size.
#include "task_log_buffer_libretiny_test.cpp"
