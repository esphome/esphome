#include "ble_service.h"
#include "ble_server.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace esp32_ble {

static const char *TAG = "esp32_ble.service";

BLEService::BLEService(ESPBTUUID uuid, uint16_t num_handles, uint8_t inst_id)
    : uuid_(uuid), num_handles_(num_handles), inst_id_(inst_id) {
  this->create_lock_ = xSemaphoreCreateBinary();
  this->start_lock_ = xSemaphoreCreateBinary();
  this->stop_lock_ = xSemaphoreCreateBinary();

  xSemaphoreGive(this->create_lock_);
  xSemaphoreGive(this->start_lock_);
  xSemaphoreGive(this->stop_lock_);
}

BLEService::~BLEService() {
  for (auto &chr : this->characteristics_)
    delete chr;
}

BLECharacteristic *BLEService::get_characteristic(ESPBTUUID uuid) {
  for (auto *chr : this->characteristics_)
    if (chr->get_uuid() == uuid)
      return chr;
  return nullptr;
}

BLECharacteristic *BLEService::get_characteristic(uint16_t uuid) {
  return this->get_characteristic(ESPBTUUID::from_uint16(uuid));
}
BLECharacteristic *BLEService::create_characteristic(uint16_t uuid, esp_gatt_char_prop_t properties) {
  return create_characteristic(ESPBTUUID::from_uint16(uuid), properties);
}
BLECharacteristic *BLEService::create_characteristic(const std::string uuid, esp_gatt_char_prop_t properties) {
  return create_characteristic(ESPBTUUID::from_raw(uuid), properties);
}
BLECharacteristic *BLEService::create_characteristic(ESPBTUUID uuid, esp_gatt_char_prop_t properties) {
  BLECharacteristic *characteristic = new BLECharacteristic(uuid, properties);
  this->characteristics_.push_back(characteristic);
  return characteristic;
}

bool BLEService::do_create(BLEServer *server) {
  this->server_ = server;

  xSemaphoreTake(this->create_lock_, SEMAPHORE_MAX_DELAY);
  esp_gatt_srvc_id_t srvc_id;
  srvc_id.is_primary = true;
  srvc_id.id.inst_id = this->inst_id_;
  srvc_id.id.uuid = this->uuid_.get_uuid();

  esp_err_t err = esp_ble_gatts_create_service(server->get_gatts_if(), &srvc_id, this->num_handles_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gatts_create_service failed: %d", err);
    return false;
  }
  xSemaphoreWait(this->create_lock_, SEMAPHORE_MAX_DELAY);

  return true;
}

void BLEService::start() {
  for (auto *characteristic : this->characteristics_) {
    this->last_created_characteristic_ = characteristic;
    characteristic->do_create(this);
  }

  xSemaphoreTake(this->start_lock_, SEMAPHORE_MAX_DELAY);
  esp_err_t err = esp_ble_gatts_start_service(this->handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gatts_start_service failed: %d", err);
    return;
  }
  xSemaphoreWait(this->start_lock_, SEMAPHORE_MAX_DELAY);
}

void BLEService::stop() {
  xSemaphoreTake(this->stop_lock_, SEMAPHORE_MAX_DELAY);
  esp_err_t err = esp_ble_gatts_stop_service(this->handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gatts_stop_service failed: %d", err);
    return;
  }
  xSemaphoreWait(this->stop_lock_, SEMAPHORE_MAX_DELAY);
}

void BLEService::gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                     esp_ble_gatts_cb_param_t *param) {
  switch (event) {
    case ESP_GATTS_CREATE_EVT: {
      if (this->uuid_ == ESPBTUUID::from_uuid(param->create.service_id.id.uuid) &&
          this->inst_id_ == param->create.service_id.id.inst_id) {
        this->handle_ = param->create.service_handle;
        xSemaphoreGive(this->create_lock_);
      }
      break;
    }
    case ESP_GATTS_START_EVT: {
      if (param->start.service_handle == this->handle_) {
        xSemaphoreGive(this->start_lock_);
      }
      break;
    }
    case ESP_GATTS_STOP_EVT: {
      if (param->start.service_handle == this->handle_) {
        xSemaphoreGive(this->stop_lock_);
      }
      break;
    }
    default:
      break;
  }

  for (auto *characteristic : this->characteristics_) {
    characteristic->gatts_event_handler(event, gatts_if, param);
  }
}

}  // namespace esp32_ble
}  // namespace esphome

#endif
