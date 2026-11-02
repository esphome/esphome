#include "reboot.h"

#include "esphome/core/log.h"

namespace esphome::duco {

ESPHOME_LOG_TAG(TAG, "duco.reboot");

void DucoReboot::press_action() {
  if (!this->parent_->allows_dangerous_controls()) {
    ESP_LOGW(TAG, "Reboot rejected: allow_dangerous_controls is disabled");
    return;
  }

  if (this->address_ == 0) {
    this->parent_->send(nullptr, 0x04, {0x00});
  } else {
    for (uint8_t i = 0; i < 11; i++) {
      this->parent_->send(nullptr, FUNCTION_NODE_DATA, {0x08, this->address_});
    }
  }
}

}  // namespace esphome::duco
