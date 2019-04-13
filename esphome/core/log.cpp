#include "esphome/core/log.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

namespace esphome {

int HOT esp_log_printf_(int level, const char *tag, const char *format, ...) {  // NOLINT
  va_list arg;
  va_start(arg, format);
  int ret = esp_log_vprintf_(level, tag, format, arg);
  va_end(arg);
  return ret;
}
#ifdef USE_STORE_LOG_STR_IN_FLASH
int HOT esp_log_printf_(int level, const char *tag, const __FlashStringHelper *format, ...) {
  va_list arg;
  va_start(arg, format);
  int ret = esp_log_vprintf_(level, tag, format, arg);
  va_end(arg);
  return ret;
  return 0;
}
#endif

int HOT esp_log_vprintf_(int level, const char *tag, const char *format, va_list args) {  // NOLINT
#ifdef USE_LOGGER
  auto *log = logger::global_logger;
  if (log == nullptr)
    return 0;

  return log->log_vprintf_(level, tag, format, args);
#else
  return 0;
#endif
}

#ifdef USE_STORE_LOG_STR_IN_FLASH
int HOT esp_log_vprintf_(int level, const char *tag, const __FlashStringHelper *format, va_list args) {  // NOLINT
#ifdef USE_LOGGER
  auto *log = logger::global_logger;
  if (log == nullptr)
    return 0;

  return log->log_vprintf_(level, tag, format, args);
#else
  return 0;
#endif
}
#endif

int HOT esp_idf_log_vprintf_(const char *format, va_list args) {  // NOLINT
#ifdef USE_LOGGER
  auto *log = logger::global_logger;
  if (log == nullptr)
    return 0;

  return log->log_vprintf_(log->get_global_log_level(), "", format, args);
#else
  return 0;
#endif
}

}  // namespace esphome
