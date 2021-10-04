#include "restart_switch.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace restart {

static const char *const TAG = "restart";

void RestartSwitch::write_state(bool state) {
  // Acknowledge
  this->publish_state(false);

  if (state) {
    ESP_LOGI(TAG, "Restarting device...");
    // Let MQTT settle a bit
    delay(100);  // NOLINT
    App.safe_reboot();
  }
}
void RestartSwitch::dump_config() { LOG_SWITCH("", "Restart Switch", this); }

}  // namespace restart
}  // namespace esphome
