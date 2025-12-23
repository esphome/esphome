#include "neewer_ble_client.h"
#include "utils.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace neewerlight_ct {

using utils::TAG;

const std::string NeewerBleClient::SERVICE_UUID = "69400001-B5A3-F393-E0A9-E50E24DCCA99";
const std::string NeewerBleClient::CHARACTERISTIC_UUID = "69400002-B5A3-F393-E0A9-E50E24DCCA99";

void NeewerBleClient::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                          esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT:
      this->client_state_ = esp32_ble_tracker::ClientState::ESTABLISHED;
      ESP_LOGW(TAG, "[%s] Connected successfully!", this->characteristic_uuid_.to_string().c_str());
      break;
    case ESP_GATTC_DISCONNECT_EVT:
      ESP_LOGW(TAG, "[%s] Disconnected", this->characteristic_uuid_.to_string().c_str());
      this->client_state_ = esp32_ble_tracker::ClientState::IDLE;
      break;
    case ESP_GATTC_WRITE_CHAR_EVT:
      ESP_LOGD(TAG, "[%s] Got ESP_GATTC_WRITE_CHAR_EVT status = %d", this->characteristic_uuid_.to_string().c_str(),
               param->write.status);
      break;
    default:
      break;
  }
}

void NeewerBleClient::send_message(esphome::ble_client::BLEClient *client, std::vector<uint8_t> &&msg) {
  if (client == nullptr) {
    ESP_LOGW(TAG, "BLE parent is null. Can not get characteristic to write to.");
    return;
  }

  if (this->client_state_ != esp32_ble_tracker::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "[%s] Not connected to BLE client. State update can not be written.",
             this->characteristic_uuid_.to_string().c_str());
    return;
  }

  esp32_ble_client::BLECharacteristic *characteristic =
      client->get_characteristic(this->service_uuid_, this->characteristic_uuid_);
  if (characteristic == nullptr) {
    ESP_LOGW(TAG, "[%s] Characteristic not found. State update can not be written.",
             this->characteristic_uuid_.to_string().c_str());
    return;
  }

  if (msg.empty()) {
    ESP_LOGW(TAG, "Empty message. No data will be written.");
    return;
  }
  ESP_LOGD(TAG, "[%s] Writing %u bytes", this->characteristic_uuid_.to_string().c_str(), msg.size());
  for (unsigned int i = 0; i < msg.size(); i++) {
    ESP_LOGV(TAG, "Byte %i/%u to write: %i (0x%1x)", i, msg.size() - 1, msg[i], msg[i]);
  }
  esp_err_t status = characteristic->write_value(msg.data(), msg.size(), ESP_GATT_WRITE_TYPE_RSP);
  if (status != ESP_OK) {
    ESP_LOGW(TAG, "[%s] Write error, esp_err_t status = %d", this->characteristic_uuid_.to_string().c_str(), status);
  } else {
    ESP_LOGD(TAG, "[%s] Write successful", this->characteristic_uuid_.to_string().c_str());
  }
}

}  // namespace neewerlight_ct
}  // namespace esphome

#endif  // USE_ESP32
