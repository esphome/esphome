#include "ble_characteristic.h"
#include "ble_service.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace esp32_ble {

static const char *TAG = "esp32_ble.characteristic";

void BLECharacteristic::set_value(std::string value) {
  xSemaphoreTake(this->set_value_lock_, 0L);
  this->value_ = value;
  xSemaphoreGive(this->set_value_lock_);
}
void BLECharacteristic::set_value(uint8_t *data, size_t length) { this->set_value(std::string((char *) data, length)); }
void BLECharacteristic::set_value(uint8_t &data) {
  uint8_t temp[1];
  temp[0] = data;
  this->set_value(temp, 1);
}
void BLECharacteristic::set_value(uint16_t &data) {
  uint8_t temp[2];
  temp[0] = data;
  temp[1] = data >> 8;
  this->set_value(temp, 2);
}
void BLECharacteristic::set_value(uint32_t &data) {
  uint8_t temp[4];
  temp[0] = data;
  temp[1] = data >> 8;
  temp[2] = data >> 16;
  temp[3] = data >> 24;
  this->set_value(temp, 4);
}
void BLECharacteristic::set_value(int &data) {
  uint8_t temp[4];
  temp[0] = data;
  temp[1] = data >> 8;
  temp[2] = data >> 16;
  temp[3] = data >> 24;
  this->set_value(temp, 4);
}
void BLECharacteristic::set_value(float &data) {
  float temp = data;
  this->set_value((uint8_t *) &temp, 4);
}
void BLECharacteristic::set_value(double &data) {
  double temp = data;
  this->set_value((uint8_t *) &temp, 8);
}

bool BLECharacteristic::do_create(BLEService *service) {
  this->service_ = service;
  esp_attr_control_t control;
  control.auto_rsp = ESP_GATT_RSP_BY_APP;

  ESP_LOGD(TAG, "Creating characteristic");

  esp_bt_uuid_t uuid = this->uuid_.get_uuid();
  esp_err_t err = esp_ble_gatts_add_char(service->get_handle(), &uuid, static_cast<esp_gatt_perm_t>(this->permissions_),
                                         this->properties_, nullptr, &control);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gatts_add_char failed: %d", err);
    return false;
  }

  // Create descriptors
  return true;
}

void BLECharacteristic::gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                            esp_ble_gatts_cb_param_t *param) {
  switch (event) {
    case ESP_GATTS_ADD_CHAR_EVT: {
      if (this->uuid_ == ESPBTUUID::from_uuid(param->add_char.char_uuid)) {
        this->handle_ = param->add_char.attr_handle;
        this->created_ = true;
      }
      break;
    }
    default:
      break;
  }

  // Pass event to descriptors
}

}  // namespace esp32_ble
}  // namespace esphome

#endif
