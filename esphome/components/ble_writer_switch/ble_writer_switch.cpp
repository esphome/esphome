#include "ble_writer_switch.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_writer_switch {

static const char *const TAG = "ble_writer_switch";

void BLEWriterSwitch::write_state(bool state) {
  if (this->handle_ == 0) {
    ESP_LOGW(TAG, "Cannot write to BLE characteristic, handle == 0");
    return;
  }
  ESP_LOGVV(TAG, "Will publish state %d", state);
  this->publish_state(state);
  uint8_t data[1] = {state};
  esp_ble_gattc_write_char(this->parent()->gattc_if, this->parent()->conn_id, this->handle_, sizeof(data), data,
                           ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
}

void BLEWriterSwitch::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                          esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_REG_EVT:
      this->publish_state(this->parent_->enabled);
      break;
    case ESP_GATTC_OPEN_EVT:
      this->node_state = espbt::ClientState::ESTABLISHED;
      break;
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      this->handle_ = 0;
      auto chr = this->parent()->get_characteristic(this->service_uuid_, this->char_uuid_);
      if (chr == nullptr) {
        this->status_set_warning();
        this->publish_state(NAN);
        ESP_LOGW(TAG, "No sensor characteristic found at service %s char %s", this->service_uuid_.to_string().c_str(),
                 this->char_uuid_.to_string().c_str());
        break;
      }
      this->handle_ = chr->handle;
      this->node_state = espbt::ClientState::ESTABLISHED;
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT:
      this->node_state = espbt::ClientState::IDLE;
      this->publish_state(this->parent_->enabled);
      break;
    default:
      break;
  }
}

void BLEWriterSwitch::dump_config() { LOG_SWITCH("", "BLE Writer Switch", this); }

}  // namespace ble_writer_switch
}  // namespace esphome
#endif
