#include "ble_service.h"
#include "ble_client_base.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace esp32_ble_client {

static const char *const TAG = "esp32_ble_client.service";

BLECharacteristic *BLEService::get_characteristic(espbt::ESPBTUUID uuid) {
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

void BLEService::parse_characteristics() {
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
      ESP_LOGW(TAG, "esp_ble_gattc_get_all_char error, status=%d", status);
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
    ESP_LOGI(TAG, " characteristic %s, handle 0x%x, properties 0x%x", characteristic->uuid.to_string().c_str(),
             characteristic->handle, characteristic->properties);
    characteristic->parse_descriptors();
    offset++;
  }
}

}  // namespace esp32_ble_client
}  // namespace esphome

#endif  // USE_ESP32
