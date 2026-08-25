#include "ble_server.h"

#include "esphome/components/esp32_ble/ble.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"
#include "esphome/core/version.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include <nvs_flash.h>
#include <freertos/FreeRTOSConfig.h>
#include <esp_bt_main.h>
#ifndef CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID
#include <esp_bt.h>
#endif
#include <freertos/task.h>
#include <esp_gap_ble_api.h>

namespace esphome::esp32_ble_server {

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
        size_t write_idx = 0;
        for (auto *service : this->services_to_start_) {
          if (service->is_created()) {
            service->start();  // Needs to be called once per characteristic in the service
          }
          // Remove services that have started or are starting
          if (!service->is_starting() && !service->is_running()) {
            this->services_to_start_[write_idx++] = service;
          }
        }
        this->services_to_start_.erase(this->services_to_start_.begin() + write_idx, this->services_to_start_.end());
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
        for (auto &entry : this->services_) {
          if (entry.service == this->device_information_service_) {
            continue;
          }
          entry.service->do_create(this);
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

bool BLEServer::can_proceed() { return this->is_running() || !this->parent_->is_active(); }

void BLEServer::restart_advertising_() {
  if (this->is_running()) {
    this->parent_->advertising_set_manufacturer_data(this->manufacturer_data_);
  }
}

BLEService *BLEServer::create_service(ESPBTUUID uuid, bool advertise, uint16_t num_handles) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char uuid_buf[esp32_ble::UUID_STR_LEN];
  uuid.to_str(uuid_buf);
  ESP_LOGV(TAG, "Creating BLE service - %s", uuid_buf);
#endif
  // Calculate the inst_id for the service
  uint8_t inst_id = 0;
  for (; inst_id < 0xFF; inst_id++) {
    if (this->get_service(uuid, inst_id) == nullptr) {
      break;
    }
  }
  if (inst_id == 0xFF) {
    char warn_uuid_buf[esp32_ble::UUID_STR_LEN];
    uuid.to_str(warn_uuid_buf);
    ESP_LOGW(TAG, "Could not create BLE service %s, too many instances", warn_uuid_buf);
    return nullptr;
  }
  BLEService *service =  // NOLINT(cppcoreguidelines-owning-memory)
      new BLEService(uuid, num_handles, inst_id, advertise);
  this->services_.push_back({uuid, inst_id, service});
  if (this->parent_->is_active() && this->registered_) {
    service->do_create(this);
  }
  return service;
}

void BLEServer::remove_service(ESPBTUUID uuid, uint8_t inst_id) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char uuid_buf[esp32_ble::UUID_STR_LEN];
  uuid.to_str(uuid_buf);
  ESP_LOGV(TAG, "Removing BLE service - %s %d", uuid_buf, inst_id);
#endif
  for (auto it = this->services_.begin(); it != this->services_.end(); ++it) {
    if (it->uuid == uuid && it->inst_id == inst_id) {
      it->service->do_delete();
      delete it->service;  // NOLINT(cppcoreguidelines-owning-memory)
      this->services_.erase(it);
      return;
    }
  }
  char warn_uuid_buf[esp32_ble::UUID_STR_LEN];
  uuid.to_str(warn_uuid_buf);
  ESP_LOGW(TAG, "BLE service %s %d does not exist", warn_uuid_buf, inst_id);
}

BLEService *BLEServer::get_service(ESPBTUUID uuid, uint8_t inst_id) {
  for (auto &entry : this->services_) {
    if (entry.uuid == uuid && entry.inst_id == inst_id) {
      return entry.service;
    }
  }
  return nullptr;
}

void BLEServer::dispatch_callbacks_(CallbackType type, uint16_t conn_id) {
  for (auto &entry : this->callbacks_) {
    if (entry.type == type) {
      entry.callback(conn_id);
    }
  }
}

void BLEServer::gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                    esp_ble_gatts_cb_param_t *param) {
  switch (event) {
    case ESP_GATTS_CONNECT_EVT: {
      ESP_LOGD(TAG, "BLE Client connected");
      this->add_client_(param->connect.conn_id);
      // Resume advertising so additional clients can discover and connect
      if (this->client_count_ < this->max_clients_) {
        this->parent_->advertising_start();
      }
      this->dispatch_callbacks_(CallbackType::ON_CONNECT, param->connect.conn_id);
      break;
    }
    case ESP_GATTS_DISCONNECT_EVT: {
      ESP_LOGD(TAG, "BLE Client disconnected");
      this->remove_client_(param->disconnect.conn_id);
      this->parent_->advertising_start();
      this->dispatch_callbacks_(CallbackType::ON_DISCONNECT, param->disconnect.conn_id);
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

  for (auto &entry : this->services_) {
    entry.service->gatts_event_handler(event, gatts_if, param);
  }
}

int8_t BLEServer::find_client_index_(uint16_t conn_id) const {
  for (uint8_t i = 0; i < this->client_count_; i++) {
    if (this->clients_[i] == conn_id)
      return i;
  }
  return -1;
}

void BLEServer::add_client_(uint16_t conn_id) {
  // Check if already in list
  if (this->find_client_index_(conn_id) >= 0)
    return;
  // Add if there's space
  if (this->client_count_ < USE_ESP32_BLE_MAX_CONNECTIONS) {
    this->clients_[this->client_count_++] = conn_id;
  } else {
    // This should never happen since max clients is known at compile time
    ESP_LOGE(TAG, "Client array full");
  }
}

void BLEServer::remove_client_(uint16_t conn_id) {
  int8_t index = this->find_client_index_(conn_id);
  if (index >= 0) {
    // Replace with last element and decrement count (client order not preserved)
    this->clients_[index] = this->clients_[--this->client_count_];
  }
}

void BLEServer::ble_before_disabled_event_handler() {
  // Delete all clients
  this->client_count_ = 0;
  // Delete all services
  for (auto &entry : this->services_) {
    entry.service->do_delete();
  }
  this->registered_ = false;
  this->state_ = INIT;
}

void BLEServer::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
#ifdef ESPHOME_ESP32_BLE_EXTENDED_AUTH_PARAMS
  switch (event) {
    case ESP_GAP_BLE_SEC_REQ_EVT:
      if (this->passkey_) {
        esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &this->passkey_, sizeof(uint32_t));
      }
      esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, this->pairing_enabled_);
      break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
      esp_bd_addr_t bd_addr;
      memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
      ESP_LOGW(TAG, "remote BD_ADDR: %08x%04x",
               (bd_addr[0] << 24) + (bd_addr[1] << 16) + (bd_addr[2] << 8) + bd_addr[3],
               (bd_addr[4] << 8) + bd_addr[5]);
      ESP_LOGW(TAG, "address type = %d", param->ble_security.auth_cmpl.addr_type);
      ESP_LOGW(TAG, "pair status = %s", param->ble_security.auth_cmpl.success ? "success" : "fail");
      break;
#ifdef USE_ESP32_BLE_SERVER_ON_PASSKEY_REQUEST
    case ESP_GAP_BLE_PASSKEY_REQ_EVT: {
      char mac_buf[18];
      format_mac_addr_upper(param->ble_security.ble_req.bd_addr, mac_buf);
      this->passkey_request_callback_.call(mac_buf);
      break;
    }
#endif
#ifdef USE_ESP32_BLE_SERVER_ON_PASSKEY_NOTIFICATION
    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT: {
      char mac_buf[18];
      format_mac_addr_upper(param->ble_security.ble_req.bd_addr, mac_buf);
      this->passkey_notification_callback_.call(mac_buf, param->ble_security.key_notif.passkey);
      break;
    }
#endif
#ifdef USE_ESP32_BLE_SERVER_ON_NUMERIC_COMPARISON_REQUEST
    case ESP_GAP_BLE_NC_REQ_EVT: {
      char mac_buf[18];
      format_mac_addr_upper(param->ble_security.ble_req.bd_addr, mac_buf);
      this->numeric_comparison_request_callback_.call(mac_buf, param->ble_security.key_notif.passkey);
      break;
    }
#endif
    default:
      break;
  }
#endif
}

float BLEServer::get_setup_priority() const { return setup_priority::AFTER_BLUETOOTH + 10; }

void BLEServer::dump_config() {
  ESP_LOGCONFIG(TAG,
                "ESP32 BLE Server:\n"
                "  Max clients: %u",
                this->max_clients_);
}

BLEServer *global_ble_server = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::esp32_ble_server

#endif
