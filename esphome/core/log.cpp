#include "log.h"
#include "defines.h"
#include "helpers.h"

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

namespace esphome {

void HOT esp_log_printf_(int level, const char *tag, int line, const char *format, ...) {  // NOLINT
  va_list arg;
  va_start(arg, format);
  esp_log_vprintf_(level, tag, line, format, arg);
  va_end(arg);
}
#ifdef USE_STORE_LOG_STR_IN_FLASH
void HOT esp_log_printf_(int level, const char *tag, int line, const __FlashStringHelper *format, ...) {
  va_list arg;
  va_start(arg, format);
  esp_log_vprintf_(level, tag, line, format, arg);
  va_end(arg);
}
#endif

void HOT esp_log_vprintf_(int level, const char *tag, int line, const char *format, va_list args) {  // NOLINT
#ifdef USE_LOGGER
  auto *log = logger::global_logger;
  if (log == nullptr)
    return;

  log->log_vprintf_(level, tag, line, format, args);
#endif
}

#ifdef USE_STORE_LOG_STR_IN_FLASH
void HOT esp_log_vprintf_(int level, const char *tag, int line, const __FlashStringHelper *format,
                          va_list args) {  // NOLINT
#ifdef USE_LOGGER
  auto *log = logger::global_logger;
  if (log == nullptr)
    return;

  log->log_vprintf_(level, tag, line, format, args);
#endif
}
#endif

#ifdef ARDUINO_ARCH_ESP32
int HOT esp_idf_log_vprintf_(const char *format, va_list args) {  // NOLINT
#ifdef USE_LOGGER
  auto *log = logger::global_logger;
  if (log == nullptr)
    return 0;

  size_t len = strlen(format);
  if (format[len - 1] == '\n') {
    // Remove trailing newline from format
    // Use locally stored
    static std::string FORMAT_COPY;
    FORMAT_COPY.clear();
    FORMAT_COPY.insert(0, format, len - 1);
    format = FORMAT_COPY.c_str();
  }

  log->log_vprintf_(ESPHOME_LOG_LEVEL, "esp-idf", 0, format, args);
#endif
  return 0;
}
#endif

}  // namespace esphome
