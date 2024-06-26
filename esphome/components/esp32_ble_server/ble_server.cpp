#include "ble_server.h"

#include "esphome/components/esp32_ble/ble.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/version.h"

#ifdef USE_ESP32

#include <nvs_flash.h>
#include <freertos/FreeRTOSConfig.h>
#include <esp_bt_main.h>
#include <esp_bt.h>
#include <freertos/task.h>
#include <esp_gap_ble_api.h>

namespace esphome {
namespace esp32_ble_server {

static const char *const TAG = "esp32_ble_server";

static const uint16_t DEVICE_INFORMATION_SERVICE_UUID = 0x180A;
static const uint16_t MODEL_UUID = 0x2A24;
static const uint16_t VERSION_UUID = 0x2A26;
static const uint16_t MANUFACTURER_UUID = 0x2A29;

void BLEServer::setup() {
  if (this->parent_->is_failed()) {
    this->mark_failed();
    ESP_LOGE(TAG, "BLE Server was marked failed by ESP32BLE");
    return;
  }
  global_ble_server = this;
}

void BLEServer::loop() {
  if (!this->parent_->is_active()) {
    return;
  }
  switch (this->state_) {
    case RUNNING:
      return;

    case INIT: {
      esp_err_t err = esp_ble_gatts_app_register(0);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gatts_app_register failed: %d", err);
        this->mark_failed();
        return;
      }
      this->state_ = REGISTERING;
      break;
    }
    case REGISTERING: {
      if (this->registered_) {
        // Create all services previously created
        for (auto &pair : this->services_) {
          pair.second->do_create(this);
        }
        if (this->device_information_service_ == nullptr) {
          this->create_service(ESPBTUUID::from_uint16(DEVICE_INFORMATION_SERVICE_UUID));
          this->device_information_service_ =
              this->get_service(ESPBTUUID::from_uint16(DEVICE_INFORMATION_SERVICE_UUID));
          this->create_device_characteristics_();
        }
        this->state_ = STARTING_SERVICE;
      }
      break;
    }
    case STARTING_SERVICE: {
      if (!this->device_information_service_->is_created()) {
        break;
      }
      if (this->device_information_service_->is_running()) {
        this->state_ = RUNNING;
        this->restart_advertising_();
        ESP_LOGD(TAG, "BLE server setup successfully");
      } else if (!this->device_information_service_->is_starting()) {
        this->device_information_service_->start();
      }
      break;
    }
  }
}

bool BLEServer::is_running() { return this->parent_->is_active() && this->state_ == RUNNING; }

bool BLEServer::can_proceed() { return this->is_running() || !this->parent_->is_active(); }

void BLEServer::restart_advertising_() {
  if (this->is_running()) {
    this->parent_->advertising_set_manufacturer_data(this->manufacturer_data_);
  }
}

bool BLEServer::create_device_characteristics_() {
  if (this->model_.has_value()) {
    BLECharacteristic *model =
        this->device_information_service_->create_characteristic(MODEL_UUID, BLECharacteristic::PROPERTY_READ);
    model->set_value(this->model_.value());
  } else {
    BLECharacteristic *model =
        this->device_information_service_->create_characteristic(MODEL_UUID, BLECharacteristic::PROPERTY_READ);
    model->set_value(ESPHOME_BOARD);
  }

  BLECharacteristic *version =
      this->device_information_service_->create_characteristic(VERSION_UUID, BLECharacteristic::PROPERTY_READ);
  version->set_value("ESPHome " ESPHOME_VERSION);

  BLECharacteristic *manufacturer =
      this->device_information_service_->create_characteristic(MANUFACTURER_UUID, BLECharacteristic::PROPERTY_READ);
  manufacturer->set_value(this->manufacturer_);

  return true;
}

void BLEServer::create_service(ESPBTUUID uuid, bool advertise, uint16_t num_handles, uint8_t inst_id) {
  ESP_LOGV(TAG, "Creating BLE service - %s", uuid.to_string().c_str());
  // If the service already exists, do nothing
  BLEService *service = this->get_service(uuid);
  if (service != nullptr) {
    ESP_LOGW(TAG, "BLE service %s already exists", uuid.to_string().c_str());
    return;
  }
  service = new BLEService(uuid, num_handles, inst_id, advertise);  // NOLINT(cppcoreguidelines-owning-memory)
  this->services_.emplace(uuid.to_string(), service);
  service->do_create(this);
}

void BLEServer::remove_service(ESPBTUUID uuid) {
  ESP_LOGV(TAG, "Removing BLE service - %s", uuid.to_string().c_str());
  BLEService *service = this->get_service(uuid);
  if (service == nullptr) {
    ESP_LOGW(TAG, "BLE service %s not found", uuid.to_string().c_str());
    return;
  }
  service->do_delete();
  delete service;  // NOLINT(cppcoreguidelines-owning-memory)
  this->services_.erase(uuid.to_string());
}

BLEService *BLEServer::get_service(ESPBTUUID uuid) {
  BLEService *service = nullptr;
  if (this->services_.count(uuid.to_string()) > 0) {
    service = this->services_.at(uuid.to_string());
  }
  return service;
}

void BLEServer::gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                    esp_ble_gatts_cb_param_t *param) {
  switch (event) {
    case ESP_GATTS_CONNECT_EVT: {
      ESP_LOGD(TAG, "BLE Client connected");
      this->add_client_(param->connect.conn_id, (void *) this);
      this->connected_clients_++;
      for (auto *component : this->service_components_) {
        component->on_client_connect();
      }
      break;
    }
    case ESP_GATTS_DISCONNECT_EVT: {
      ESP_LOGD(TAG, "BLE Client disconnected");
      if (this->remove_client_(param->disconnect.conn_id))
        this->connected_clients_--;
      this->parent_->advertising_start();
      for (auto *component : this->service_components_) {
        component->on_client_disconnect();
      }
      break;
    }
    case ESP_GATTS_REG_EVT: {
      this->gatts_if_ = gatts_if;
      this->registered_ = true;
      break;
    }
    default:
      break;
  }

  for (const auto &pair : this->services_) {
    pair.second->gatts_event_handler(event, gatts_if, param);
  }
}

void BLEServer::ble_before_disabled_event_handler() {
  // Delete all clients
  this->clients_.clear();
  // Delete all services
  for (auto &pair : this->services_) {
    pair.second->do_delete();
  }
  this->registered_ = false;
  this->state_ = INIT;
}

float BLEServer::get_setup_priority() const { return setup_priority::AFTER_BLUETOOTH + 10; }

void BLEServer::dump_config() { ESP_LOGCONFIG(TAG, "ESP32 BLE Server:"); }

BLEServer *global_ble_server = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esp32_ble_server
}  // namespace esphome

#endif
