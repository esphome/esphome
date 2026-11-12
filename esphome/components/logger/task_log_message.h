#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESPHOME_TASK_LOG_BUFFER
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace esphome::logger {

#if defined(USE_ESP32) || defined(USE_LIBRETINY)
// Header of a variable-size task log message; the text follows it
struct TaskLogMessage {
  const char *tag;       // We store the pointer, assuming tags are static
  char thread_name[16];  // Copied, the task may be gone before the main loop reads it
  uint16_t text_length;
  uint16_t line;
  uint8_t level;

  inline char *text_data() { return reinterpret_cast<char *>(this) + sizeof(TaskLogMessage); }
};
#endif

#if defined(USE_ESP32) || defined(USE_LIBRETINY) || defined(USE_ZEPHYR)
static constexpr int TASK_LOG_MAX_TEXT_SIZE = 255;

// Capped text length; <= 0 on error or empty text
inline int task_log_text_length(const char *format, va_list args) {
  va_list args_copy;
  va_copy(args_copy, args);
  int ret = vsnprintf(nullptr, 0, format, args_copy);
  va_end(args_copy);
  return ret > TASK_LOG_MAX_TEXT_SIZE ? TASK_LOG_MAX_TEXT_SIZE : ret;
}
#endif

template<typename Message>
inline void fill_task_log_header(Message *msg, uint8_t level, const char *tag, uint16_t line, const char *thread_name) {
  msg->level = level;
  msg->tag = tag;
  msg->line = line;
  if (thread_name != nullptr) {
    strncpy(msg->thread_name, thread_name, sizeof(msg->thread_name) - 1);
    msg->thread_name[sizeof(msg->thread_name) - 1] = '\0';
  } else {
    msg->thread_name[0] = '\0';
  }
}

inline uint16_t trim_trailing_newlines(const char *text, uint16_t length) {
  while (length > 0 && text[length - 1] == '\n')
    length--;
  return length;
}

}  // namespace esphome::logger

#endif  // USE_ESPHOME_TASK_LOG_BUFFER
