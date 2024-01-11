#include "ble_binary_output.h"
#include "esphome/core/log.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#ifdef USE_ESP32
namespace esphome {
namespace ble_client {

static const char *const TAG = "ble_binary_output";

void BLEBinaryOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "BLE Binary Output:");
  ESP_LOGCONFIG(TAG, "  MAC address        : %s", this->parent_->address_str().c_str());
  ESP_LOGCONFIG(TAG, "  Service UUID       : %s", this->service_uuid_.to_string().c_str());
  ESP_LOGCONFIG(TAG, "  Characteristic UUID: %s", this->char_uuid_.to_string().c_str());
  LOG_BINARY_OUTPUT(this);
}

void BLEBinaryOutput::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                          esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto *chr = this->parent()->get_characteristic(this->service_uuid_, this->char_uuid_);
      if (chr == nullptr) {
        ESP_LOGW(TAG, "Characteristic %s was not found in service %s", this->char_uuid_.to_string().c_str(),
                 this->service_uuid_.to_string().c_str());
        break;
      }
      this->char_handle_ = chr->handle;
      this->char_props_ = chr->properties;
      if (this->require_response_ && this->char_props_ & ESP_GATT_CHAR_PROP_BIT_WRITE) {
        this->write_type_ = ESP_GATT_WRITE_TYPE_RSP;
        ESP_LOGD(TAG, "Write type: ESP_GATT_WRITE_TYPE_RSP");
      } else if (!this->require_response_ && this->char_props_ & ESP_GATT_CHAR_PROP_BIT_WRITE_NR) {
        this->write_type_ = ESP_GATT_WRITE_TYPE_NO_RSP;
        ESP_LOGD(TAG, "Write type: ESP_GATT_WRITE_TYPE_NO_RSP");
      } else {
        ESP_LOGE(TAG, "Characteristic %s does not allow writing with%s response", this->char_uuid_.to_string().c_str(),
                 this->require_response_ ? "" : "out");
        break;
      }
      this->node_state = espbt::ClientState::ESTABLISHED;
      ESP_LOGD(TAG, "Found characteristic %s on device %s", this->char_uuid_.to_string().c_str(),
               this->parent()->address_str().c_str());
      this->node_state = espbt::ClientState::ESTABLISHED;
      break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT: {
      if (param->write.handle == this->char_handle_) {
        if (param->write.status != 0)
          ESP_LOGW(TAG, "[%s] Write error, status=%d", this->char_uuid_.to_string().c_str(), param->write.status);
      }
      break;
    }
    default:
      break;
  }
}

void BLEBinaryOutput::write_state(bool state) {
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "[%s] Not connected to BLE client.  State update can not be written.",
             this->char_uuid_.to_string().c_str());
    return;
  }
  uint8_t state_as_uint = (uint8_t) state;
  ESP_LOGV(TAG, "[%s] Write State: %d", this->char_uuid_.to_string().c_str(), state_as_uint);
  esp_err_t err =
      esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(), this->char_handle_,
                               sizeof(state_as_uint), &state_as_uint, this->write_type_, ESP_GATT_AUTH_REQ_NONE);
  if (err != ESP_GATT_OK)
    ESP_LOGW(TAG, "[%s] Write error, err=%d", this->char_uuid_.to_string().c_str(), err);
}

}  // namespace ble_client
}  // namespace esphome
#endif
