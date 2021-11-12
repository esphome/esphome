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
    case ESP_GATTC_OPEN_EVT:
      this->client_state_ = espbt::ClientState::ESTABLISHED;
      ESP_LOGW(TAG, "[%s] Connected successfully!", this->char_uuid_.to_string().c_str());
      break;
    case ESP_GATTC_DISCONNECT_EVT:
      ESP_LOGW(TAG, "[%s] Disconnected", this->char_uuid_.to_string().c_str());
      this->client_state_ = espbt::ClientState::IDLE;
      break;
    case ESP_GATTC_WRITE_CHAR_EVT: {
      if (param->write.status == 0) {
        break;
      }

      auto chr = this->parent()->get_characteristic(this->service_uuid_, this->char_uuid_);
      if (chr == nullptr) {
        ESP_LOGW(TAG, "[%s] Characteristic not found.", this->char_uuid_.to_string().c_str());
        break;
      }
      if (param->write.handle == chr->handle) {
        ESP_LOGW(TAG, "[%s] Write error, status=%d", this->char_uuid_.to_string().c_str(), param->write.status);
      }
      break;
    }
    default:
      break;
  }
}

void BLEBinaryOutput::write_state(bool state) {
  if (this->client_state_ != espbt::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "[%s] Not connected to BLE client.  State update can not be written.",
             this->char_uuid_.to_string().c_str());
    return;
  }

  auto chr = this->parent()->get_characteristic(this->service_uuid_, this->char_uuid_);
  if (chr == nullptr) {
    ESP_LOGW(TAG, "[%s] Characteristic not found.  State update can not be written.",
             this->char_uuid_.to_string().c_str());
    return;
  }

  uint8_t state_as_uint = (uint8_t) state;
  ESP_LOGV(TAG, "[%s] Write State: %d", this->char_uuid_.to_string().c_str(), state_as_uint);
  chr->write_value(&state_as_uint, sizeof(state_as_uint));
}

}  // namespace ble_client
}  // namespace esphome
#endif
