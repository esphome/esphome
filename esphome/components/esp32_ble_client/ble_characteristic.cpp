#include "ble_characteristic.h"
#include "ble_client_base.h"
#include "ble_service.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace esp32_ble_client {

static const char *const TAG = "esp32_ble_client";

BLECharacteristic::~BLECharacteristic() {
  for (auto &desc : this->descriptors)
    delete desc;  // NOLINT(cppcoreguidelines-owning-memory)
}

void BLECharacteristic::release_descriptors() {
  this->parsed = false;
  for (auto &desc : this->descriptors)
    delete desc;  // NOLINT(cppcoreguidelines-owning-memory)
  this->descriptors.clear();
}

void BLECharacteristic::parse_descriptors() {
  this->parsed = true;
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
      ESP_LOGW(TAG, "[%d] [%s] esp_ble_gattc_get_all_descr error, status=%d",
               this->service->client->get_connection_index(), this->service->client->address_str().c_str(), status);
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
    ESP_LOGV(TAG, "[%d] [%s]    descriptor %s, handle 0x%x", this->service->client->get_connection_index(),
             this->service->client->address_str().c_str(), desc->uuid.to_string().c_str(), desc->handle);
    offset++;
  }
}

BLEDescriptor *BLECharacteristic::get_descriptor(espbt::ESPBTUUID uuid) {
  if (!this->parsed)
    this->parse_descriptors();
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
  if (!this->parsed)
    this->parse_descriptors();
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
    ESP_LOGW(TAG, "[%d] [%s] Error sending write value to BLE gattc server, status=%d",
             this->service->client->get_connection_index(), this->service->client->address_str().c_str(), status);
  }
  return status;
}

esp_err_t BLECharacteristic::write_value(uint8_t *new_val, int16_t new_val_size) {
  return write_value(new_val, new_val_size, ESP_GATT_WRITE_TYPE_NO_RSP);
}

}  // namespace esp32_ble_client
}  // namespace esphome

#endif  // USE_ESP32
