#include "log.h"
#include "defines.h"
#include "helpers.h"
#include <cstdio>

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

namespace esphome {

#ifdef ESPHOME_DEBUG
static void early_log_printf_(const char *tag, int line, const char *format, va_list args) {
  fprintf(stderr, "LOG BEFORE LOGGER INIT [%s:%d]: ", tag, line);
  vfprintf(stderr, format, args);
  fputc('\n', stderr);
  assert(false && "log called before Logger::pre_setup()");  // NOLINT
}
#endif

void HOT esp_log_printf_(int level, const char *tag, int line, const char *format, ...) {  // NOLINT
#ifdef USE_LOGGER
#ifdef ESPHOME_DEBUG
  if (logger::global_logger == nullptr) {
    va_list arg;
    va_start(arg, format);
    early_log_printf_(tag, line, format, arg);
    va_end(arg);
    return;
  }
#endif
  va_list arg;
  va_start(arg, format);
  logger::global_logger->log_vprintf_(static_cast<uint8_t>(level), tag, line, format, arg);
  va_end(arg);
#endif
}

#ifdef USE_STORE_LOG_STR_IN_FLASH
void HOT esp_log_printf_(int level, const char *tag, int line, const __FlashStringHelper *format, ...) {
#ifdef USE_LOGGER
  ESPHOME_DEBUG_ASSERT(logger::global_logger != nullptr);
  va_list arg;
  va_start(arg, format);
  logger::global_logger->log_vprintf_(static_cast<uint8_t>(level), tag, line, format, arg);
  va_end(arg);
#endif
}
#endif

void HOT esp_log_vprintf_(int level, const char *tag, int line, const char *format, va_list args) {  // NOLINT
#ifdef USE_LOGGER
#ifdef ESPHOME_DEBUG
  if (logger::global_logger == nullptr) {
    early_log_printf_(tag, line, format, args);
    return;
  }
#endif
  logger::global_logger->log_vprintf_(static_cast<uint8_t>(level), tag, line, format, args);
#endif
}

#ifdef USE_STORE_LOG_STR_IN_FLASH
// Remove before 2026.9.0
void HOT esp_log_vprintf_(int level, const char *tag, int line, const __FlashStringHelper *format, va_list args) {
#ifdef USE_LOGGER
  ESPHOME_DEBUG_ASSERT(logger::global_logger != nullptr);
  logger::global_logger->log_vprintf_(static_cast<uint8_t>(level), tag, line, format, args);
#endif
}
#endif

#ifdef USE_ESP32
int HOT esp_idf_log_vprintf_(const char *format, va_list args) {  // NOLINT
#ifdef USE_LOGGER
#ifdef ESPHOME_DEBUG
  if (logger::global_logger == nullptr) {
    early_log_printf_("esp-idf", 0, format, args);
    return 0;
  }
#endif
  logger::global_logger->log_vprintf_(ESPHOME_LOG_LEVEL, "esp-idf", 0, format, args);
#endif
  return 0;
}
#endif

}  // namespace esphome
