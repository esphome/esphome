#include "log.h"
#include "defines.h"
#include "helpers.h"

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

namespace esphome {

// Call log_vprintf_ directly to avoid extra indirection through esp_log_vprintf_
void HOT esp_log_printf_(int level, const char *tag, int line, const char *format, ...) {  // NOLINT
#ifdef USE_LOGGER
  auto *log = logger::global_logger;
  if (log == nullptr)
    return;

  va_list arg;
  va_start(arg, format);
  log->log_vprintf_(static_cast<uint8_t>(level), tag, line, format, arg);
  va_end(arg);
#endif
}
#ifdef USE_STORE_LOG_STR_IN_FLASH
void HOT esp_log_printf_(int level, const char *tag, int line, const __FlashStringHelper *format, ...) {
#ifdef USE_LOGGER
  auto *log = logger::global_logger;
  if (log == nullptr)
    return;

  va_list arg;
  va_start(arg, format);
  log->log_vprintf_(static_cast<uint8_t>(level), tag, line, format, arg);
  va_end(arg);
#endif
}
#endif

void HOT esp_log_vprintf_(int level, const char *tag, int line, const char *format, va_list args) {  // NOLINT
#ifdef USE_LOGGER
  auto *log = logger::global_logger;
  if (log == nullptr)
    return;

  log->log_vprintf_(static_cast<uint8_t>(level), tag, line, format, args);
#endif
}

#ifdef USE_ESP32
int HOT esp_idf_log_vprintf_(const char *format, va_list args) {  // NOLINT
#ifdef USE_LOGGER
  auto *log = logger::global_logger;
  if (log == nullptr)
    return 0;

  log->log_vprintf_(ESPHOME_LOG_LEVEL, "esp-idf", 0, format, args);
#endif
  return 0;
}
#endif

}  // namespace esphome
