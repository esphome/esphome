#include "safe_mode_switch.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace safe_mode {

static const char *const TAG = "safe_mode_switch";

void SafeModeSwitch::set_ota(ota::OTAComponent *ota) { this->ota_ = ota; }

void SafeModeSwitch::write_state(bool state) {
  // Acknowledge
  this->publish_state(false);

  if (state) {
    ESP_LOGI(TAG, "Restarting device in safe mode...");
    this->ota_->set_safe_mode_pending(true);

    // Let MQTT settle a bit
    delay(100);  // NOLINT
    App.safe_reboot();
  }
}
void SafeModeSwitch::dump_config() { LOG_SWITCH("", "Safe Mode Switch", this); }

}  // namespace safe_mode
}  // namespace esphome
