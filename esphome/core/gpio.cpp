#include "esphome/core/gpio.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>

namespace esphome {

#ifdef USE_ESP8266

static constexpr size_t LOG_PIN_PREFIX_MAX_LEN = 32;

void log_pin(const char *tag, const __FlashStringHelper *prefix, GPIOPin *pin) {
  if (pin == nullptr)
    return;
  char buffer[GPIO_SUMMARY_MAX_LEN];
  size_t len = pin->dump_summary(buffer, sizeof(buffer));
  // Clamp to actual buffer size (snprintf returns would-be length)
  len = std::min(len, sizeof(buffer) - 1);
  // Copy prefix from flash to stack, then format
  char prefix_buf[LOG_PIN_PREFIX_MAX_LEN];
  strncpy_P(prefix_buf, reinterpret_cast<const char *>(prefix), sizeof(prefix_buf) - 1);
  prefix_buf[sizeof(prefix_buf) - 1] = '\0';
  esp_log_printf_(ESPHOME_LOG_LEVEL_CONFIG, tag, __LINE__, "%s%.*s", prefix_buf, (int) len, buffer);
}

#else

void log_pin(const char *tag, const char *prefix, GPIOPin *pin) {
  if (pin == nullptr)
    return;
  char buffer[GPIO_SUMMARY_MAX_LEN];
  size_t len = pin->dump_summary(buffer, sizeof(buffer));
  // Clamp to actual buffer size (snprintf returns would-be length)
  len = std::min(len, sizeof(buffer) - 1);
  esp_log_printf_(ESPHOME_LOG_LEVEL_CONFIG, tag, __LINE__, "%s%.*s", prefix, (int) len, buffer);
}

#endif

}  // namespace esphome
