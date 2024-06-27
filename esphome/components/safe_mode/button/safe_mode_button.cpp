#include "safe_mode_button.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace safe_mode {

static const char *const TAG = "safe_mode.button";

void SafeModeButton::set_safe_mode(SafeModeComponent *safe_mode_component) {
  this->safe_mode_component_ = safe_mode_component;
}

void SafeModeButton::press_action() {
  ESP_LOGI(TAG, "Restarting device in safe mode...");
  this->safe_mode_component_->set_safe_mode_pending(true);

  // Let MQTT settle a bit
  delay(100);  // NOLINT
  App.safe_reboot();
}

void SafeModeButton::dump_config() { LOG_BUTTON("", "Safe Mode Button", this); }

}  // namespace safe_mode
}  // namespace esphome
