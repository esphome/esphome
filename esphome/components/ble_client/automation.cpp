#include "automation.h"

#include <esp_bt_defs.h>
#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>

#include "esphome/core/log.h"

namespace esphome {
namespace ble_client {
static const char *const TAG = "ble_client.automation";

void BLEWriterClientNode::write(const std::vector<uint8_t> &value) {
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "Cannot write to BLE characteristic - not connected");
    return;
  } else if (this->ble_char_handle_ == 0) {
    ESP_LOGW(TAG, "Cannot write to BLE characteristic - characteristic not found");
    return;
  }
  esp_gatt_write_type_t write_type;
  if (this->char_props_ & ESP_GATT_CHAR_PROP_BIT_WRITE) {
    write_type = ESP_GATT_WRITE_TYPE_RSP;
    ESP_LOGD(TAG, "Write type: ESP_GATT_WRITE_TYPE_RSP");
  } else if (this->char_props_ & ESP_GATT_CHAR_PROP_BIT_WRITE_NR) {
    write_type = ESP_GATT_WRITE_TYPE_NO_RSP;
    ESP_LOGD(TAG, "Write type: ESP_GATT_WRITE_TYPE_NO_RSP");
  } else {
    ESP_LOGE(TAG, "Characteristic %s does not allow writing", this->char_uuid_.to_string().c_str());
    return;
  }
  ESP_LOGVV(TAG, "Will write %d bytes: %s", value.size(), format_hex_pretty(value).c_str());
  esp_err_t err =
      esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(), this->ble_char_handle_,
                               value.size(), const_cast<uint8_t *>(value.data()), write_type, ESP_GATT_AUTH_REQ_NONE);
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
      ESP_LOGD(TAG, "Connection established with %s", ble_client_->address_str().c_str());
      break;
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto *chr = this->parent()->get_characteristic(this->service_uuid_, this->char_uuid_);
      if (chr == nullptr) {
        ESP_LOGW("ble_write_action", "Characteristic %s was not found in service %s",
                 this->char_uuid_.to_string().c_str(), this->service_uuid_.to_string().c_str());
        break;
      }
      this->ble_char_handle_ = chr->handle;
      this->char_props_ = chr->properties;
      this->node_state = espbt::ClientState::ESTABLISHED;
      ESP_LOGD(TAG, "Found characteristic %s on device %s", this->char_uuid_.to_string().c_str(),
               ble_client_->address_str().c_str());
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT:
      this->node_state = espbt::ClientState::IDLE;
      this->ble_char_handle_ = 0;
      ESP_LOGD(TAG, "Disconnected from %s", ble_client_->address_str().c_str());
      break;
    default:
      break;
  }
}

}  // namespace ble_client
}  // namespace esphome
