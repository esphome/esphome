#include "reset_switch.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace reset {

static const char *const TAG = "reset.switch";

void ResetSwitch::dump_config() { LOG_SWITCH("", "Reset Switch", this); }
void ResetSwitch::write_state(bool state) {
  // Acknowledge
  this->publish_state(false);

  if (state) {
    ESP_LOGI(TAG, "Resetting to defaults...");
    // Let MQTT settle a bit
    delay(100);  // NOLINT
    global_preferences->reset();
    App.safe_reboot();
  }
}

}  // namespace reset
}  // namespace esphome
