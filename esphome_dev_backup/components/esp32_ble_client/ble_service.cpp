#include "ble_service.h"
#include "ble_client_base.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace esp32_ble_client {

static const char *const TAG = "esp32_ble_client";

BLECharacteristic *BLEService::get_characteristic(espbt::ESPBTUUID uuid) {
  if (!this->parsed)
    this->parse_characteristics();
  for (auto &chr : this->characteristics) {
    if (chr->uuid == uuid)
      return chr;
  }
  return nullptr;
}

BLECharacteristic *BLEService::get_characteristic(uint16_t uuid) {
  return this->get_characteristic(espbt::ESPBTUUID::from_uint16(uuid));
}

BLEService::~BLEService() {
  for (auto &chr : this->characteristics)
    delete chr;  // NOLINT(cppcoreguidelines-owning-memory)
}

void BLEService::release_characteristics() {
  this->parsed = false;
  for (auto &chr : this->characteristics)
    delete chr;  // NOLINT(cppcoreguidelines-owning-memory)
  this->characteristics.clear();
}

void BLEService::parse_characteristics() {
  this->parsed = true;
  uint16_t offset = 0;
  esp_gattc_char_elem_t result;

  while (true) {
    uint16_t count = 1;
    esp_gatt_status_t status =
        esp_ble_gattc_get_all_char(this->client->get_gattc_if(), this->client->get_conn_id(), this->start_handle,
                                   this->end_handle, &result, &count, offset);
    if (status == ESP_GATT_INVALID_OFFSET || status == ESP_GATT_NOT_FOUND) {
      break;
    }
    if (status != ESP_GATT_OK) {
      ESP_LOGW(TAG, "[%d] [%s] esp_ble_gattc_get_all_char error, status=%d", this->client->get_connection_index(),
               this->client->address_str().c_str(), status);
      break;
    }
    if (count == 0) {
      break;
    }

    BLECharacteristic *characteristic = new BLECharacteristic();  // NOLINT(cppcoreguidelines-owning-memory)
    characteristic->uuid = espbt::ESPBTUUID::from_uuid(result.uuid);
    characteristic->properties = result.properties;
    characteristic->handle = result.char_handle;
    characteristic->service = this;
    this->characteristics.push_back(characteristic);
    ESP_LOGV(TAG, "[%d] [%s]  characteristic %s, handle 0x%x, properties 0x%x", this->client->get_connection_index(),
             this->client->address_str().c_str(), characteristic->uuid.to_string().c_str(), characteristic->handle,
             characteristic->properties);
    offset++;
  }
}

}  // namespace esp32_ble_client
}  // namespace esphome

#endif  // USE_ESP32
