#include "automation.h"

#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <esp_bt_defs.h>

#include "esphome/core/log.h"

namespace esphome {
namespace ble_client {
static const char *const TAG = "ble_client.automation";

void BLEWriterClientNode::write() {
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "Cannot write to BLE characteristic - not connected");
    return;
  } else if (this->ble_char_handle_ == 0) {
    ESP_LOGW(TAG, "Cannot write to BLE characteristic - characteristic not found");
    return;
  }
  ESP_LOGVV(TAG, "Will write %d bytes: %s", this->value_.size(), format_hex_pretty(this->value_).c_str());
  esp_err_t err =
      esp_ble_gattc_write_char(this->parent()->gattc_if, this->parent()->conn_id, this->ble_char_handle_, value_.size(),
                               value_.data(), ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error writing to characteristic: %s!", esp_err_to_name(err));
  }
}

void BLEWriterClientNode::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                              esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_REG_EVT:
      break;
    case ESP_GATTC_OPEN_EVT:
      this->node_state = espbt::ClientState::ESTABLISHED;
      break;
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto *chr = this->parent()->get_characteristic(this->service_uuid_, this->char_uuid_);
      if (chr == nullptr) {
        ESP_LOGW("ble_write_action", "Characteristic %s was not found in service %s",
                 this->char_uuid_.to_string().c_str(), this->service_uuid_.to_string().c_str());
        break;
      }
      this->ble_char_handle_ = chr->handle;
      this->node_state = espbt::ClientState::ESTABLISHED;
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT:
      this->node_state = espbt::ClientState::IDLE;
      this->ble_char_handle_ = 0;
      break;
    default:
      break;
  }
}

}  // namespace ble_client
}  // namespace esphome
