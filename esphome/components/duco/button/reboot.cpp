#include "reboot.h"

#include "esphome/core/log.h"

namespace esphome::duco {

ESPHOME_LOG_TAG(TAG, "duco.reboot");

void DucoReboot::press_action() {
  if (!this->parent_->allows_dangerous_controls()) {
    ESP_LOGW(TAG, "Reboot rejected: allow_dangerous_controls is disabled");
    return;
  }

  this->parent_->send(nullptr, 0x04, {0x00});
}

}  // namespace esphome::duco
