#include "ble_service.h"
#include "ble_server.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace esp32_ble_server {

static const char *const TAG = "esp32_ble_server.service";

BLEService::BLEService(ESPBTUUID uuid, uint16_t num_handles, uint8_t inst_id)
    : uuid_(uuid), num_handles_(num_handles), inst_id_(inst_id) {}

BLEService::~BLEService() {
  for (auto &chr : this->characteristics_)
    delete chr;  // NOLINT(cppcoreguidelines-owning-memory)
}

BLECharacteristic *BLEService::get_characteristic(ESPBTUUID uuid) {
  for (auto *chr : this->characteristics_) {
    if (chr->get_uuid() == uuid)
      return chr;
  }
  return nullptr;
}

BLECharacteristic *BLEService::get_characteristic(uint16_t uuid) {
  return this->get_characteristic(ESPBTUUID::from_uint16(uuid));
}
BLECharacteristic *BLEService::create_characteristic(uint16_t uuid, esp_gatt_char_prop_t properties) {
  return create_characteristic(ESPBTUUID::from_uint16(uuid), properties);
}
BLECharacteristic *BLEService::create_characteristic(const std::string &uuid, esp_gatt_char_prop_t properties) {
  return create_characteristic(ESPBTUUID::from_raw(uuid), properties);
}
BLECharacteristic *BLEService::create_characteristic(ESPBTUUID uuid, esp_gatt_char_prop_t properties) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  BLECharacteristic *characteristic = new BLECharacteristic(uuid, properties);
  this->characteristics_.push_back(characteristic);
  return characteristic;
}

void BLEService::do_create(BLEServer *server) {
  this->server_ = server;

  esp_gatt_srvc_id_t srvc_id;
  srvc_id.is_primary = true;
  srvc_id.id.inst_id = this->inst_id_;
  srvc_id.id.uuid = this->uuid_.get_uuid();

  esp_err_t err = esp_ble_gatts_create_service(server->get_gatts_if(), &srvc_id, this->num_handles_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gatts_create_service failed: %d", err);
    this->init_state_ = FAILED;
    return;
  }
  this->init_state_ = CREATING;
}

bool BLEService::do_create_characteristics_() {
  if (this->created_characteristic_count_ >= this->characteristics_.size() &&
      (this->last_created_characteristic_ == nullptr || this->last_created_characteristic_->is_created()))
    return false;  // Signifies there are no characteristics, or they are all finished being created.

  if (this->last_created_characteristic_ != nullptr && !this->last_created_characteristic_->is_created())
    return true;  // Signifies that the previous characteristic is still being created.

  auto *characteristic = this->characteristics_[this->created_characteristic_count_++];
  this->last_created_characteristic_ = characteristic;
  characteristic->do_create(this);
  return true;
}

void BLEService::start() {
  if (this->do_create_characteristics_())
    return;

  esp_err_t err = esp_ble_gatts_start_service(this->handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gatts_start_service failed: %d", err);
    return;
  }
  this->running_state_ = STARTING;
}

void BLEService::stop() {
  esp_err_t err = esp_ble_gatts_stop_service(this->handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gatts_stop_service failed: %d", err);
    return;
  }
  this->running_state_ = STOPPING;
}

bool BLEService::is_created() { return this->init_state_ == CREATED; }
bool BLEService::is_failed() {
  if (this->init_state_ == FAILED)
    return true;
  bool failed = false;
  for (auto *characteristic : this->characteristics_)
    failed |= characteristic->is_failed();

  if (failed)
    this->init_state_ = FAILED;
  return this->init_state_ == FAILED;
}

void BLEService::gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                     esp_ble_gatts_cb_param_t *param) {
  switch (event) {
    case ESP_GATTS_CREATE_EVT: {
      if (this->uuid_ == ESPBTUUID::from_uuid(param->create.service_id.id.uuid) &&
          this->inst_id_ == param->create.service_id.id.inst_id) {
        this->handle_ = param->create.service_handle;
        this->init_state_ = CREATED;
      }
      break;
    }
    case ESP_GATTS_START_EVT: {
      if (param->start.service_handle == this->handle_) {
        this->running_state_ = RUNNING;
      }
      break;
    }
    case ESP_GATTS_STOP_EVT: {
      if (param->start.service_handle == this->handle_) {
        this->running_state_ = STOPPED;
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

}  // namespace esp32_ble_server
}  // namespace esphome

#endif
