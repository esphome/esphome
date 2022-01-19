#include "restart_button.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace restart {

static const char *const TAG = "restart.button";

void RestartButton::press_action() {
  ESP_LOGI(TAG, "Restarting device...");
  // Let MQTT settle a bit
  delay(100);  // NOLINT
  App.safe_reboot();
}
void RestartButton::dump_config() { LOG_BUTTON("", "Restart Button", this); }

}  // namespace restart
}  // namespace esphome
