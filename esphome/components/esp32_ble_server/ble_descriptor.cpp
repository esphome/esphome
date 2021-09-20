#include "ble_descriptor.h"
#include "ble_characteristic.h"
#include "ble_service.h"
#include "esphome/core/log.h"

#include <cstring>

#ifdef USE_ESP32

namespace esphome {
namespace esp32_ble_server {

static const char *const TAG = "esp32_ble_server.descriptor";

BLEDescriptor::BLEDescriptor(ESPBTUUID uuid, uint16_t max_len) {
  this->uuid_ = uuid;
  this->value_.attr_len = 0;
  this->value_.attr_max_len = max_len;
  this->value_.attr_value = (uint8_t *) malloc(max_len);  // NOLINT
}

BLEDescriptor::~BLEDescriptor() { free(this->value_.attr_value); }  // NOLINT

void BLEDescriptor::do_create(BLECharacteristic *characteristic) {
  this->characteristic_ = characteristic;
  esp_attr_control_t control;
  control.auto_rsp = ESP_GATT_AUTO_RSP;

  ESP_LOGV(TAG, "Creating descriptor - %s", this->uuid_.to_string().c_str());
  esp_bt_uuid_t uuid = this->uuid_.get_uuid();
  esp_err_t err = esp_ble_gatts_add_char_descr(this->characteristic_->get_service()->get_handle(), &uuid,
                                               this->permissions_, &this->value_, &control);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gatts_add_char_descr failed: %d", err);
    this->state_ = FAILED;
    return;
  }
  this->state_ = CREATING;
}

void BLEDescriptor::set_value(const std::string &value) { this->set_value((uint8_t *) value.data(), value.length()); }
void BLEDescriptor::set_value(const uint8_t *data, size_t length) {
  if (length > this->value_.attr_max_len) {
    ESP_LOGE(TAG, "Size %d too large, must be no bigger than %d", length, this->value_.attr_max_len);
    return;
  }
  this->value_.attr_len = length;
  memcpy(this->value_.attr_value, data, length);
}

void BLEDescriptor::gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param) {
  switch (event) {
    case ESP_GATTS_ADD_CHAR_DESCR_EVT: {
      if (this->characteristic_ != nullptr && this->uuid_ == ESPBTUUID::from_uuid(param->add_char_descr.descr_uuid) &&
          this->characteristic_->get_service()->get_handle() == param->add_char_descr.service_handle &&
          this->characteristic_ == this->characteristic_->get_service()->get_last_created_characteristic()) {
        this->handle_ = param->add_char_descr.attr_handle;
        this->state_ = CREATED;
      }
      break;
    }
    case ESP_GATTS_WRITE_EVT: {
      if (this->handle_ == param->write.handle) {
        this->value_.attr_len = param->write.len;
        memcpy(this->value_.attr_value, param->write.value, param->write.len);
      }
      break;
    }
    default:
      break;
  }
}

}  // namespace esp32_ble_server
}  // namespace esphome

#endif
