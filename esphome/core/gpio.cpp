#include "esphome/core/gpio.h"
#include "esphome/core/log.h"

namespace esphome {

void log_pin(const char *tag, const char *prefix, GPIOPin *pin) {
  if (pin == nullptr)
    return;
  char buffer[GPIO_SUMMARY_MAX_LEN];
  pin->dump_summary(buffer, sizeof(buffer));
  esp_log_printf_(ESPHOME_LOG_LEVEL_CONFIG, tag, __LINE__, "%s%s", prefix, buffer);
}

}  // namespace esphome
