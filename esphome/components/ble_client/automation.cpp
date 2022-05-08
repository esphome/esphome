#include "automation.h"

#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <esp_bt_defs.h>

#include "esphome/core/log.h"

namespace esphome {
namespace ble_client {
static const char *const TAG = "ble_client.automation";

// Attempts to write to the configured characteristic. Returns false on retriable errors and true on success or
// non-retriable errors.
bool BLEWriterClientNode::write() {
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "Cannot write to BLE characteristic - not connected");
    return false;
  } else if (this->ble_char_handle_ == 0) {
    ESP_LOGW(TAG, "Cannot write to BLE characteristic - characteristic not found");
    return false;
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
    // Non-retriable error, likely due to user configuration error.
    return true;
  }
  ESP_LOGD(TAG, "Will write %d bytes: %s", this->value_.size(), format_hex_pretty(this->value_).c_str());
  esp_err_t err = esp_ble_gattc_write_char(this->parent()->gattc_if, this->parent()->conn_id, this->ble_char_handle_,
                                           value_.size(), value_.data(), write_type, ESP_GATT_AUTH_REQ_NONE);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error writing to characteristic: %s!", esp_err_to_name(err));
    return false;
  }

  ESP_LOGD(TAG, "Success writing to characteristic");
  return true;
}

void BLEWriterClientNode::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                              esp_ble_gattc_cb_param_t *param) {
  switch (event) {
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

void BLEWriterClientNode::loop() {
  if (pending_write_ && this->node_state == espbt::ClientState::ESTABLISHED) {
    ESP_LOGD(TAG, "There is a pending write on an established connection. Attempting to write.");

    if (!write()) {
      ESP_LOGD(TAG, "Error writing value. Will retry.");
      return;
    }

    pending_write_ = false;

    // If the BLEClient should not maintain an active connection, tell it to disconnect.
    if (!ble_client_->get_maintain_connection()) {
      ESP_LOGD(TAG, "Telling BLEClient to disconnect");
      ble_client_->set_should_connect(false);
    }
  }
}

}  // namespace ble_client
}  // namespace esphome
