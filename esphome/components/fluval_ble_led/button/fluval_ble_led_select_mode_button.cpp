#include "fluval_ble_led_select_mode_button.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace fluval_ble_led {

static const char *const TAG = "fluval_ble_led_select_mode_button";

void FluvalBleLedSelectModeButton::setup() { ESP_LOGD(TAG, "Setup called"); }

void FluvalBleLedSelectModeButton::press_action() {
  ESP_LOGVV(TAG, "BUTTON PRESSED: %d ", this->trigger_mode_);
  this->parent_->set_mode(this->trigger_mode_);
}

void FluvalBleLedSelectModeButton::dump_config() { LOG_BUTTON("", "Fluval BLE LED Select Mode Button", this); }

}  // namespace fluval_ble_led
}  // namespace esphome

#endif
