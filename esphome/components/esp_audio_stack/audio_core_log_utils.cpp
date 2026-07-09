#include "audio_core_log_utils.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

#include <cstdarg>
#include <cstdio>

namespace esphome::esp_audio_stack {

namespace {

void log_vprintf(int level, const char *tag, const char *format, va_list args) {
  char buffer[192];
  vsnprintf(buffer, sizeof(buffer), format, args);
  switch (level) {
    case ESPHOME_LOG_LEVEL_CONFIG:
      ::esphome::esp_log_printf_(ESPHOME_LOG_LEVEL_CONFIG, tag, __LINE__, "%s", buffer);
      break;
    case ESPHOME_LOG_LEVEL_INFO:
      ::esphome::esp_log_printf_(ESPHOME_LOG_LEVEL_INFO, tag, __LINE__, "%s", buffer);
      break;
    case ESPHOME_LOG_LEVEL_WARN:
      ::esphome::esp_log_printf_(ESPHOME_LOG_LEVEL_WARN, tag, __LINE__, "%s", buffer);
      break;
    default:
      ::esphome::esp_log_printf_(ESPHOME_LOG_LEVEL_ERROR, tag, __LINE__, "%s", buffer);
      break;
  }
}

}  // namespace

void log_config(const char *tag, const char *format, ...) {
  va_list args;
  va_start(args, format);
  log_vprintf(ESPHOME_LOG_LEVEL_CONFIG, tag, format, args);
  va_end(args);
}

void log_info(const char *tag, const char *format, ...) {
  va_list args;
  va_start(args, format);
  log_vprintf(ESPHOME_LOG_LEVEL_INFO, tag, format, args);
  va_end(args);
}

void log_warn(const char *tag, const char *format, ...) {
  va_list args;
  va_start(args, format);
  log_vprintf(ESPHOME_LOG_LEVEL_WARN, tag, format, args);
  va_end(args);
}

void log_error(const char *tag, const char *format, ...) {
  va_list args;
  va_start(args, format);
  log_vprintf(ESPHOME_LOG_LEVEL_ERROR, tag, format, args);
  va_end(args);
}

}  // namespace esphome::esp_audio_stack

#endif  // USE_ESP32
