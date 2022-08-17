#include "reset_button.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace reset {

static const char *const TAG = "reset.button";

void ResetButton::dump_config() { LOG_BUTTON("", "Reset Button", this); }
void ResetButton::press_action() {
  ESP_LOGI(TAG, "Resetting to defaults...");
  // Let MQTT settle a bit
  delay(100);  // NOLINT
  global_preferences->reset();
  ESP_LOGI(TAG, "Rebooting...");
  App.safe_reboot();
}

}  // namespace reset
}  // namespace esphome
