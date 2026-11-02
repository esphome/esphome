#include "installer_mode.h"

#include "esphome/core/log.h"

namespace esphome::duco {

ESPHOME_LOG_TAG(TAG, "duco.installer_mode");

void DucoInstallerMode::write_state(bool state) {
  if (!this->parent_->allows_dangerous_controls()) {
    ESP_LOGW(TAG, "Installer mode change rejected: allow_dangerous_controls is disabled");
    this->publish_state(this->state);
    return;
  }

  this->parent_->send(nullptr, 0x10, {0x00, static_cast<uint8_t>(state)});
  // Update state immediately
  this->publish_state(state);
}

}  // namespace esphome::duco
