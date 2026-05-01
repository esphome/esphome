#if defined(USE_HOST)
#include "logger.h"

namespace esphome::logger {

void HOT Logger::write_msg_(const char *msg, uint16_t len) {
  static constexpr size_t TIMESTAMP_LEN = 10;  // "[HH:MM:SS]"
  static constexpr size_t HEADROOM = 128;      // Extra space for ANSI codes, newline, etc.
  char buffer[TIMESTAMP_LEN + ESPHOME_LOGGER_TX_BUFFER_SIZE + HEADROOM];

  time_t rawtime;
  time(&rawtime);
  struct tm timeinfo;
  localtime_r(&rawtime, &timeinfo);  // Thread-safe version
  size_t pos = strftime(buffer, TIMESTAMP_LEN + 1, "[%H:%M:%S]", &timeinfo);

  // Copy message (with newline already included by caller)
  size_t copy_len = std::min(static_cast<size_t>(len), sizeof(buffer) - pos);
  memcpy(buffer + pos, msg, copy_len);
  pos += copy_len;

  // Single write for everything
  fwrite(buffer, 1, pos, stdout);
}

void Logger::pre_setup() { global_logger = this; }

}  // namespace esphome::logger

#endif
