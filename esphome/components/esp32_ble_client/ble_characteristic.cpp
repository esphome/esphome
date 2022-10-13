#include "ble_characteristic.h"
#include "ble_client_base.h"
#include "ble_service.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace esp32_ble_client {

static const char *const TAG = "esp32_ble_client.characteristic";

BLECharacteristic::~BLECharacteristic() {
  for (auto &desc : this->descriptors)
    delete desc;  // NOLINT(cppcoreguidelines-owning-memory)
}

void BLECharacteristic::parse_descriptors() {
  uint16_t offset = 0;
  esp_gattc_descr_elem_t result;

  while (true) {
    uint16_t count = 1;
    esp_gatt_status_t status =
        esp_ble_gattc_get_all_descr(this->service->client->get_gattc_if(), this->service->client->get_conn_id(),
                                    this->handle, &result, &count, offset);
    if (status == ESP_GATT_INVALID_OFFSET || status == ESP_GATT_NOT_FOUND) {
      break;
    }
    if (status != ESP_GATT_OK) {
      ESP_LOGW(TAG, "esp_ble_gattc_get_all_descr error, status=%d", status);
      break;
    }
    if (count == 0) {
      break;
    }

    BLEDescriptor *desc = new BLEDescriptor();  // NOLINT(cppcoreguidelines-owning-memory)
    desc->uuid = espbt::ESPBTUUID::from_uuid(result.uuid);
    desc->handle = result.handle;
    desc->characteristic = this;
    this->descriptors.push_back(desc);
    ESP_LOGV(TAG, "   descriptor %s, handle 0x%x", desc->uuid.to_string().c_str(), desc->handle);
    offset++;
  }
}

BLEDescriptor *BLECharacteristic::get_descriptor(espbt::ESPBTUUID uuid) {
  for (auto &desc : this->descriptors) {
    if (desc->uuid == uuid)
      return desc;
  }
  return nullptr;
}
BLEDescriptor *BLECharacteristic::get_descriptor(uint16_t uuid) {
  return this->get_descriptor(espbt::ESPBTUUID::from_uint16(uuid));
}
BLEDescriptor *BLECharacteristic::get_descriptor_by_handle(uint16_t handle) {
  for (auto &desc : this->descriptors) {
    if (desc->handle == handle)
      return desc;
  }
  return nullptr;
}

esp_err_t BLECharacteristic::write_value(uint8_t *new_val, int16_t new_val_size, esp_gatt_write_type_t write_type) {
  auto *client = this->service->client;
  auto status = esp_ble_gattc_write_char(client->get_gattc_if(), client->get_conn_id(), this->handle, new_val_size,
                                         new_val, write_type, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "Error sending write value to BLE gattc server, status=%d", status);
  }
  return status;
}

esp_err_t BLECharacteristic::write_value(uint8_t *new_val, int16_t new_val_size) {
  return write_value(new_val, new_val_size, ESP_GATT_WRITE_TYPE_NO_RSP);
}

}  // namespace esp32_ble_client
}  // namespace esphome

#endif  // USE_ESP32
