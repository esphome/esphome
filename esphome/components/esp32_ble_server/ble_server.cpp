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
    case RUNNING: {
      // Start all services that are pending to start
      if (!this->services_to_start_.empty()) {
        uint16_t index_to_remove = 0;
        // Iterate over the services to start
        for (unsigned i = 0; i < this->services_to_start_.size(); i++) {
          BLEService *service = this->services_to_start_[i];
          if (service->is_created()) {
            service->start();  // Needs to be called once per characteristic in the service
          } else {
            index_to_remove = i + 1;
          }
        }
        // Remove the services that have been started
        if (index_to_remove > 0) {
          this->services_to_start_.erase(this->services_to_start_.begin(),
                                         this->services_to_start_.begin() + index_to_remove - 1);
        }
      }
      break;
    }
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
        // Create the device information service first so
        // it is at the top of the GATT table
        this->device_information_service_->do_create(this);
        // Create all services previously created
        for (auto &pair : this->services_) {
          if (pair.second == this->device_information_service_) {
            continue;
          }
          pair.second->do_create(this);
        }
        this->state_ = STARTING_SERVICE;
      }
      break;
    }
    case STARTING_SERVICE: {
      if (this->device_information_service_->is_running()) {
        this->state_ = RUNNING;
        this->restart_advertising_();
        ESP_LOGD(TAG, "BLE server setup successfully");
      } else if (this->device_information_service_->is_created()) {
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

BLEService *BLEServer::create_service(ESPBTUUID uuid, bool advertise, uint16_t num_handles) {
  ESP_LOGV(TAG, "Creating BLE service - %s", uuid.to_string().c_str());
  // Calculate the inst_id for the service
  uint8_t inst_id = 0;
  for (; inst_id < 0xFF; inst_id++) {
    if (this->get_service(uuid, inst_id) == nullptr) {
      break;
    }
  }
  if (inst_id == 0xFF) {
    ESP_LOGW(TAG, "Could not create BLE service %s, too many instances", uuid.to_string().c_str());
    return nullptr;
  }
  BLEService *service =  // NOLINT(cppcoreguidelines-owning-memory)
      new BLEService(uuid, num_handles, inst_id, advertise);
  this->services_.emplace(BLEServer::get_service_key(uuid, inst_id), service);
  if (this->parent_->is_active() && this->registered_) {
    service->do_create(this);
  }
  return service;
}

void BLEServer::remove_service(ESPBTUUID uuid, uint8_t inst_id) {
  ESP_LOGV(TAG, "Removing BLE service - %s %d", uuid.to_string().c_str(), inst_id);
  BLEService *service = this->get_service(uuid, inst_id);
  if (service == nullptr) {
    ESP_LOGW(TAG, "BLE service %s %d does not exist", uuid.to_string().c_str(), inst_id);
    return;
  }
  service->do_delete();
  delete service;  // NOLINT(cppcoreguidelines-owning-memory)
  this->services_.erase(BLEServer::get_service_key(uuid, inst_id));
}

BLEService *BLEServer::get_service(ESPBTUUID uuid, uint8_t inst_id) {
  BLEService *service = nullptr;
  if (this->services_.count(BLEServer::get_service_key(uuid, inst_id)) > 0) {
    service = this->services_.at(BLEServer::get_service_key(uuid, inst_id));
  }
  return service;
}

std::string BLEServer::get_service_key(ESPBTUUID uuid, uint8_t inst_id) {
  return uuid.to_string() + std::to_string(inst_id);
}

void BLEServer::gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                    esp_ble_gatts_cb_param_t *param) {
  switch (event) {
    case ESP_GATTS_CONNECT_EVT: {
      ESP_LOGD(TAG, "BLE Client connected");
      this->add_client_(param->connect.conn_id);
      this->emit_(BLEServerEvt::EmptyEvt::ON_CONNECT, param->connect.conn_id);
      break;
    }
    case ESP_GATTS_DISCONNECT_EVT: {
      ESP_LOGD(TAG, "BLE Client disconnected");
      this->remove_client_(param->disconnect.conn_id);
      this->parent_->advertising_start();
      this->emit_(BLEServerEvt::EmptyEvt::ON_DISCONNECT, param->disconnect.conn_id);
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
