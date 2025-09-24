#include "ble_switch.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_client {

static const char *const TAG = "ble_switch";

void BLEClientSwitch::write_state(bool state) {
  this->parent_->set_enabled(state);
  this->publish_state(state);
}

void BLEClientSwitch::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                          esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_CLOSE_EVT:
      this->publish_state(this->parent_->enabled);
      break;
    case ESP_GATTC_SEARCH_CMPL_EVT:
      this->node_state = espbt::ClientState::ESTABLISHED;
      this->publish_state(this->parent_->enabled);
      break;
    default:
      break;
  }
}

void BLEClientSwitch::dump_config() { LOG_SWITCH("", "BLE Client Switch", this); }

}  // namespace ble_client
}  // namespace esphome
#endif
