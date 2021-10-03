#include "restart_switch.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace restart {

static const char *const TAG = "restart";

const void RestartSwitch::set_ota(ota::OTAComponent *ota) {
  this->ota_ = ota;
}

const void RestartSwitch::set_safe_mode(bool safe_mode) {
  this->safe_mode_ = safe_mode;
}

void RestartSwitch::write_state(bool state) {
  // Acknowledge
  this->publish_state(false);

  if (state) {
    if(this->ota_ != nullptr) {
      this->ota_->set_safe_mode_pending(this->safe_mode_);
    }

    ESP_LOGI(TAG, "Restarting device...");
    // Let MQTT settle a bit
    delay(100);  // NOLINT
    App.safe_reboot();
  }
}
void RestartSwitch::dump_config() { LOG_SWITCH("", "Restart Switch", this); }

}  // namespace restart
}  // namespace esphome
