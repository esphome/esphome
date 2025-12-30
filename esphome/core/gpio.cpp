#include "esphome/core/gpio.h"
#include "esphome/core/log.h"

#include <algorithm>

namespace esphome {

void log_pin(const char *tag, const char *prefix, GPIOPin *pin) {
  if (pin == nullptr)
    return;
  char buffer[GPIO_SUMMARY_MAX_LEN];
  size_t len = pin->dump_summary(buffer, sizeof(buffer));
  // Clamp to actual buffer size (snprintf returns would-be length)
  len = std::min(len, sizeof(buffer) - 1);
  esp_log_printf_(ESPHOME_LOG_LEVEL_CONFIG, tag, __LINE__, "%s%.*s", prefix, (int) len, buffer);
}

}  // namespace esphome
