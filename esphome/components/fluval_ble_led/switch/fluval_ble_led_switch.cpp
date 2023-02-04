#include "fluval_ble_led_switch.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace fluval_ble_led {

static const char *const TAG = "fluval_ble_led_switch";

void FluvalBleLedSwitch::setup() {
  ESP_LOGD(TAG, "Setup called");
}

void FluvalBleLedSwitch::write_state(bool state) {
  this->parent_->set_led_state(state);
  this->publish_state(state);
}

void FluvalBleLedSwitch::notify() {
  bool state = (this->parent_->getStatus().led_on_off == 1);
  ESP_LOGV(TAG, "In notify. State: %d", state);
  this->publish_state(state);
}

void FluvalBleLedSwitch::dump_config() { LOG_SWITCH("", "Fluval BLE LED Switch", this); }

}  // namespace fluval_ble_led
}  // namespace esphome

#endif
