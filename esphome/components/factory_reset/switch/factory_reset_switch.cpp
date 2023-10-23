#include "factory_reset_switch.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace factory_reset {

static const char *const TAG = "factory_reset.switch";

void FactoryResetSwitch::dump_config() { LOG_SWITCH("", "Factory Reset Switch", this); }
void FactoryResetSwitch::write_state(bool state) {
  // Acknowledge
  this->publish_state(false);

  if (state) {
    ESP_LOGI(TAG, "Resetting to factory defaults...");
    // Let MQTT settle a bit
    delay(100);  // NOLINT
    global_preferences->reset();
    App.safe_reboot();
  }
}

}  // namespace factory_reset
}  // namespace esphome
