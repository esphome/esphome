#include "ble_server.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/version.h"

#ifdef ARDUINO_ARCH_ESP32

#include <nvs_flash.h>
#include <freertos/FreeRTOSConfig.h>
#include <esp_bt_main.h>
#include <esp_bt.h>
#include <freertos/task.h>
#include <esp_gap_ble_api.h>

namespace esphome {
namespace esp32_ble {

static const char *TAG = "esp32_ble.server";

static const uint16_t DEVICE_INFORMATION_SERVICE_UUID = 0x180A;
static const uint16_t MODEL_UUID = 0x2A24;
static const uint16_t VERSION_UUID = 0x2A26;
static const uint16_t MANUFACTURER_UUID = 0x2A29;

void BLEServer::setup() {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "BLE Server was marked faile by ESP32BLE");
    return;
  }
  global_ble_server = this;
}

void BLEServer::loop() {
  uint8_t last_state = this->state_;
  switch (this->state_) {
    case UNINITIALIZED: {
      esp_err_t err = esp_ble_gatts_app_register(0);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gatts_app_register failed: %d", err);
        this->mark_failed();
        return;
      }
      this->state_ = AWAITING_REGISTRATION;
      break;
    }
    case AWAITING_REGISTRATION: {
      break;
    }
    case REGISTERED: {
      ESP_LOGD(TAG, "App registered");
      this->device_information_service = this->create_service(DEVICE_INFORMATION_SERVICE_UUID);
      this->state_ = AWAITING_SERVICE_CREATION;
      break;
    }
    case AWAITING_SERVICE_CREATION: {
      bool created = true;
      for (auto *service : this->services_) {
        created &= service->is_created();
      }
      if (created) {
        ESP_LOGD(TAG, "All %d services created", this->services_.size());
        this->create_device_characteristics_();
        for (auto *service : this->services_) {
          service->pre_start();
        }
        this->state_ = AWAITING_SERVICE_PRE_START;
      }
      break;
    }
    case AWAITING_SERVICE_PRE_START: {
      bool can_start = true;
      for (auto *service : this->services_) {
        can_start &= service->can_start();
        if (!can_start) {
          ESP_LOGW(TAG, "Cannot start service yet - %s", service->get_uuid().to_string().c_str());
        }
      }
      if (can_start) {
        ESP_LOGD(TAG, "All services can start");
        for (auto *service : this->services_) {
          service->start();
        }
        this->state_ = AWAITING_SERVICE_START;
      }
      break;
    }
    case AWAITING_SERVICE_START: {
      bool started = true;
      for (auto *service : this->services_) {
        started &= service->is_started();
      }
      if (started) {
        ESP_LOGD(TAG, "All services started");
        this->state_ = RUNNING;
      }
      break;
    }
  }
  if (last_state != this->state_) {
    ESP_LOGD(TAG, "State %d -> %d", last_state, this->state_);
  }
}

bool BLEServer::create_device_characteristics_() {
  if (this->model_.has_value()) {
    BLECharacteristic *model =
        this->device_information_service->create_characteristic(MODEL_UUID, BLECharacteristic::PROPERTY_READ);
    model->set_value(this->model_.value());
  } else {
#ifdef ARDUINO_BOARD
    BLECharacteristic *model =
        this->device_information_service->create_characteristic(MODEL_UUID, BLECharacteristic::PROPERTY_READ);
    model->set_value(ARDUINO_BOARD);
#endif
  }

  BLECharacteristic *version =
      this->device_information_service->create_characteristic(VERSION_UUID, BLECharacteristic::PROPERTY_READ);
  version->set_value("ESPHome " ESPHOME_VERSION);

  BLECharacteristic *manufacturer =
      this->device_information_service->create_characteristic(MANUFACTURER_UUID, BLECharacteristic::PROPERTY_READ);
  manufacturer->set_value(this->manufacturer_);

  BLECharacteristic *testing =
      this->device_information_service->create_characteristic(0x1234, BLECharacteristic::PROPERTY_WRITE);

  testing->on_write(
      [this](std::vector<uint8_t> &data) { ESP_LOGD(TAG, "Received write data: %s", hexencode(data).c_str()); });

  this->advertising_ = new BLEAdvertising();
  this->advertising_->set_scan_response(true);
  this->advertising_->set_min_preferred_interval(0x06);
  this->advertising_->start();

  return true;
}

BLEService *BLEServer::create_service(const uint8_t *uuid, bool advertise) {
  return this->create_service(ESPBTUUID::from_raw(uuid), advertise);
}
BLEService *BLEServer::create_service(const char *uuid, bool advertise) {
  return this->create_service(ESPBTUUID::from_raw(uuid), advertise);
}
BLEService *BLEServer::create_service(uint16_t uuid, bool advertise) {
  return this->create_service(ESPBTUUID::from_uint16(uuid), advertise);
}
BLEService *BLEServer::create_service(ESPBTUUID uuid, bool advertise, uint16_t num_handles, uint8_t inst_id) {
  BLEService *service = new BLEService(uuid, num_handles, inst_id);
  this->services_.push_back(service);
  if (advertise) {
    this->advertising_->add_service_uuid(uuid);
  }
  service->do_create(this);
  return service;
}

void BLEServer::gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                    esp_ble_gatts_cb_param_t *param) {
  switch (event) {
    case ESP_GATTS_CONNECT_EVT: {
      ESP_LOGD(TAG, "BLE Client connected");
      this->add_client_(param->connect.conn_id, (void *) this);
      this->connected_clients_++;
      break;
    }
    case ESP_GATTS_DISCONNECT_EVT: {
      ESP_LOGD(TAG, "BLE Client disconnected");
      if (this->remove_client_(param->disconnect.conn_id))
        this->connected_clients_--;
      this->advertising_->start();
      break;
    }
    case ESP_GATTS_REG_EVT: {
      this->gatts_if_ = gatts_if;
      this->state_ = REGISTERED;
      break;
    }
    default:
      break;
  }

  for (auto *service : this->services_) {
    service->gatts_event_handler(event, gatts_if, param);
  }
}

/*void BLEServer::teardown() { BLEDevice::deinit(true); }

BLEService *BLEServer::add_service(const char *uuid) {
  ESP_LOGD(TAG, "Adding new BLE service");
  BLEService *service = this->server_->createService(uuid);

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(uuid);

  return service;
}*/

float BLEServer::get_setup_priority() const { return setup_priority::BLUETOOTH - 10; }

void BLEServer::dump_config() { ESP_LOGCONFIG(TAG, "ESP32 BLE Server:"); }

BLEServer *global_ble_server = nullptr;

}  // namespace esp32_ble
}  // namespace esphome

#endif
