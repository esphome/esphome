#include "factory_reset_button.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace factory_reset {

static const char *const TAG = "factory_reset.button";

void FactoryResetButton::dump_config() { LOG_BUTTON("", "Factory Reset Button", this); }
void FactoryResetButton::press_action() {
  ESP_LOGI(TAG, "Resetting to factory defaults...");
  // Let MQTT settle a bit
  delay(100);  // NOLINT
  global_preferences->reset();
  App.safe_reboot();
}

}  // namespace factory_reset
}  // namespace esphome
