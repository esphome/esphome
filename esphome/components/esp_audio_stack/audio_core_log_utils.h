#pragma once

#ifdef USE_ESP32

#include <cstdint>

namespace esphome::esp_audio_stack {

void log_config(const char *tag, const char *format, ...);
void log_info(const char *tag, const char *format, ...);
void log_warn(const char *tag, const char *format, ...);
void log_error(const char *tag, const char *format, ...);

// Rate-limited WARN for hot audio paths: emit on the first 5 occurrences and
// then every 100th. Each expansion site owns its static counter.
//
// The macro expects a `TAG` symbol in scope, matching ESPHome logging
// conventions used by esp_audio_stack .cpp files.
#define LOG_W_THROTTLED(fmt, ...) \
  do { \
    static uint32_t _throttled_n = 0; \
    _throttled_n++; \
    if (_throttled_n <= 5 || _throttled_n % 100 == 0) { \
      esphome::esp_audio_stack::log_warn(TAG, fmt " [n=%u]", ##__VA_ARGS__, (unsigned) _throttled_n); \
    } \
  } while (0)

}  // namespace esphome::esp_audio_stack

#endif  // USE_ESP32
