#include "bluetooth_connection.h"

#include "esphome/components/api/api_pb2.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include "bluetooth_proxy.h"

namespace esphome::bluetooth_proxy {

static const char *const TAG = "bluetooth_proxy.connection";

static void fill_128bit_uuid_array(std::array<uint64_t, 2> &out, esp_bt_uuid_t uuid_source) {
  esp_bt_uuid_t uuid = espbt::ESPBTUUID::from_uuid(uuid_source).as_128bit().get_uuid();
  out[0] = ((uint64_t) uuid.uuid.uuid128[15] << 56) | ((uint64_t) uuid.uuid.uuid128[14] << 48) |
           ((uint64_t) uuid.uuid.uuid128[13] << 40) | ((uint64_t) uuid.uuid.uuid128[12] << 32) |
           ((uint64_t) uuid.uuid.uuid128[11] << 24) | ((uint64_t) uuid.uuid.uuid128[10] << 16) |
           ((uint64_t) uuid.uuid.uuid128[9] << 8) | ((uint64_t) uuid.uuid.uuid128[8]);
  out[1] = ((uint64_t) uuid.uuid.uuid128[7] << 56) | ((uint64_t) uuid.uuid.uuid128[6] << 48) |
           ((uint64_t) uuid.uuid.uuid128[5] << 40) | ((uint64_t) uuid.uuid.uuid128[4] << 32) |
           ((uint64_t) uuid.uuid.uuid128[3] << 24) | ((uint64_t) uuid.uuid.uuid128[2] << 16) |
           ((uint64_t) uuid.uuid.uuid128[1] << 8) | ((uint64_t) uuid.uuid.uuid128[0]);
}

void BluetoothConnection::dump_config() {
  ESP_LOGCONFIG(TAG, "BLE Connection:");
  BLEClientBase::dump_config();
}

void BluetoothConnection::loop() {
  BLEClientBase::loop();

  // Early return if no active connection or not in service discovery phase
  if (this->address_ == 0 || this->send_service_ < 0 || this->send_service_ > this->service_count_) {
    return;
  }

  // Handle service discovery
  this->send_service_for_discovery_();
}

void BluetoothConnection::reset_connection_(esp_err_t reason) {
  // Send disconnection notification
  this->proxy_->send_device_connection(this->address_, false, 0, reason);

  // Important: If we were in the middle of sending services, we do NOT send
  // send_gatt_services_done() here. This ensures the client knows that
  // the service discovery was interrupted and can retry. The client
  // (aioesphomeapi) implements a 30-second timeout (DEFAULT_BLE_TIMEOUT)
  // to detect incomplete service discovery rather than relying on us to
  // tell them about a partial list.
  this->set_address(0);
  this->send_service_ = DONE_SENDING_SERVICES;
  this->proxy_->send_connections_free();
}

void BluetoothConnection::send_service_for_discovery_() {
  if (this->send_service_ == this->service_count_) {
    this->send_service_ = DONE_SENDING_SERVICES;
    this->proxy_->send_gatt_services_done(this->address_);
    if (this->connection_type_ == espbt::ConnectionType::V3_WITH_CACHE ||
        this->connection_type_ == espbt::ConnectionType::V3_WITHOUT_CACHE) {
      this->release_services();
    }
    return;
  }

  // Early return if no API connection
  auto *api_conn = this->proxy_->get_api_connection();
  if (api_conn == nullptr) {
    return;
  }

  // Send next service
  esp_gattc_service_elem_t service_result;
  uint16_t service_count = 1;
  esp_gatt_status_t service_status = esp_ble_gattc_get_service(this->gattc_if_, this->conn_id_, nullptr,
                                                               &service_result, &service_count, this->send_service_);
  this->send_service_++;

  if (service_status != ESP_GATT_OK) {
    ESP_LOGE(TAG, "[%d] [%s] esp_ble_gattc_get_service error at offset=%d, status=%d", this->connection_index_,
             this->address_str().c_str(), this->send_service_ - 1, service_status);
    return;
  }

  if (service_count == 0) {
    ESP_LOGE(TAG, "[%d] [%s] esp_ble_gattc_get_service missing, service_count=%d", this->connection_index_,
             this->address_str().c_str(), service_count);
    return;
  }

  api::BluetoothGATTGetServicesResponse resp;
  resp.address = this->address_;
  auto &service_resp = resp.services[0];
  fill_128bit_uuid_array(service_resp.uuid, service_result.uuid);
  service_resp.handle = service_result.start_handle;

  // Get the number of characteristics directly with one call
  uint16_t total_char_count = 0;
  esp_gatt_status_t char_count_status =
      esp_ble_gattc_get_attr_count(this->gattc_if_, this->conn_id_, ESP_GATT_DB_CHARACTERISTIC,
                                   service_result.start_handle, service_result.end_handle, 0, &total_char_count);

  if (char_count_status == ESP_GATT_OK && total_char_count > 0) {
    // Only reserve if we successfully got a count
    service_resp.characteristics.reserve(total_char_count);
  } else if (char_count_status != ESP_GATT_OK) {
    ESP_LOGW(TAG, "[%d] [%s] Error getting characteristic count, status=%d", this->connection_index_,
             this->address_str().c_str(), char_count_status);
  }

  // Now process characteristics
  uint16_t char_offset = 0;
  esp_gattc_char_elem_t char_result;
  while (true) {  // characteristics
    uint16_t char_count = 1;
    esp_gatt_status_t char_status =
        esp_ble_gattc_get_all_char(this->gattc_if_, this->conn_id_, service_result.start_handle,
                                   service_result.end_handle, &char_result, &char_count, char_offset);
    if (char_status == ESP_GATT_INVALID_OFFSET || char_status == ESP_GATT_NOT_FOUND) {
      break;
    }
    if (char_status != ESP_GATT_OK) {
      ESP_LOGE(TAG, "[%d] [%s] esp_ble_gattc_get_all_char error, status=%d", this->connection_index_,
               this->address_str().c_str(), char_status);
      break;
    }
    if (char_count == 0) {
      break;
    }

    service_resp.characteristics.emplace_back();
    auto &characteristic_resp = service_resp.characteristics.back();
    fill_128bit_uuid_array(characteristic_resp.uuid, char_result.uuid);
    characteristic_resp.handle = char_result.char_handle;
    characteristic_resp.properties = char_result.properties;
    char_offset++;

    // Get the number of descriptors directly with one call
    uint16_t total_desc_count = 0;
    esp_gatt_status_t desc_count_status =
        esp_ble_gattc_get_attr_count(this->gattc_if_, this->conn_id_, ESP_GATT_DB_DESCRIPTOR, char_result.char_handle,
                                     service_result.end_handle, 0, &total_desc_count);

    if (desc_count_status == ESP_GATT_OK && total_desc_count > 0) {
      // Only reserve if we successfully got a count
      characteristic_resp.descriptors.reserve(total_desc_count);
    } else if (desc_count_status != ESP_GATT_OK) {
      ESP_LOGW(TAG, "[%d] [%s] Error getting descriptor count for char handle %d, status=%d", this->connection_index_,
               this->address_str().c_str(), char_result.char_handle, desc_count_status);
    }

    // Now process descriptors
    uint16_t desc_offset = 0;
    esp_gattc_descr_elem_t desc_result;
    while (true) {  // descriptors
      uint16_t desc_count = 1;
      esp_gatt_status_t desc_status = esp_ble_gattc_get_all_descr(
          this->gattc_if_, this->conn_id_, char_result.char_handle, &desc_result, &desc_count, desc_offset);
      if (desc_status == ESP_GATT_INVALID_OFFSET || desc_status == ESP_GATT_NOT_FOUND) {
        break;
      }
      if (desc_status != ESP_GATT_OK) {
        ESP_LOGE(TAG, "[%d] [%s] esp_ble_gattc_get_all_descr error, status=%d", this->connection_index_,
                 this->address_str().c_str(), desc_status);
        break;
      }
      if (desc_count == 0) {
        break;
      }

      characteristic_resp.descriptors.emplace_back();
      auto &descriptor_resp = characteristic_resp.descriptors.back();
      fill_128bit_uuid_array(descriptor_resp.uuid, desc_result.uuid);
      descriptor_resp.handle = desc_result.handle;
      desc_offset++;
    }
  }

  // Send the message (we already checked api_conn is not null at the beginning)
  api_conn->send_message(resp, api::BluetoothGATTGetServicesResponse::MESSAGE_TYPE);
}

bool BluetoothConnection::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                              esp_ble_gattc_cb_param_t *param) {
  if (!BLEClientBase::gattc_event_handler(event, gattc_if, param))
    return false;

  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT: {
      this->reset_connection_(param->disconnect.reason);
      break;
    }
    case ESP_GATTC_CLOSE_EVT: {
      this->reset_connection_(param->close.reason);
      break;
    }
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status != ESP_GATT_OK && param->open.status != ESP_GATT_ALREADY_OPEN) {
        this->reset_connection_(param->open.status);
      } else if (this->connection_type_ == espbt::ConnectionType::V3_WITH_CACHE) {
        this->proxy_->send_device_connection(this->address_, true, this->mtu_);
        this->proxy_->send_connections_free();
      }
      this->seen_mtu_or_services_ = false;
      break;
    }
    case ESP_GATTC_CFG_MTU_EVT:
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (!this->seen_mtu_or_services_) {
        // We don't know if we will get the MTU or the services first, so
        // only send the device connection true if we have already received
        // the services.
        this->seen_mtu_or_services_ = true;
        break;
      }
      this->proxy_->send_device_connection(this->address_, true, this->mtu_);
      this->proxy_->send_connections_free();
      break;
    }
    case ESP_GATTC_READ_DESCR_EVT:
    case ESP_GATTC_READ_CHAR_EVT: {
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%d] [%s] Error reading char/descriptor at handle 0x%2X, status=%d", this->connection_index_,
                 this->address_str_.c_str(), param->read.handle, param->read.status);
        this->proxy_->send_gatt_error(this->address_, param->read.handle, param->read.status);
        break;
      }
      api::BluetoothGATTReadResponse resp;
      resp.address = this->address_;
      resp.handle = param->read.handle;
      resp.set_data(param->read.value, param->read.value_len);
      this->proxy_->get_api_connection()->send_message(resp, api::BluetoothGATTReadResponse::MESSAGE_TYPE);
      break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT:
    case ESP_GATTC_WRITE_DESCR_EVT: {
      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%d] [%s] Error writing char/descriptor at handle 0x%2X, status=%d", this->connection_index_,
                 this->address_str_.c_str(), param->write.handle, param->write.status);
        this->proxy_->send_gatt_error(this->address_, param->write.handle, param->write.status);
        break;
      }
      api::BluetoothGATTWriteResponse resp;
      resp.address = this->address_;
      resp.handle = param->write.handle;
      this->proxy_->get_api_connection()->send_message(resp, api::BluetoothGATTWriteResponse::MESSAGE_TYPE);
      break;
    }
    case ESP_GATTC_UNREG_FOR_NOTIFY_EVT: {
      if (param->unreg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%d] [%s] Error unregistering notifications for handle 0x%2X, status=%d",
                 this->connection_index_, this->address_str_.c_str(), param->unreg_for_notify.handle,
                 param->unreg_for_notify.status);
        this->proxy_->send_gatt_error(this->address_, param->unreg_for_notify.handle, param->unreg_for_notify.status);
        break;
      }
      api::BluetoothGATTNotifyResponse resp;
      resp.address = this->address_;
      resp.handle = param->unreg_for_notify.handle;
      this->proxy_->get_api_connection()->send_message(resp, api::BluetoothGATTNotifyResponse::MESSAGE_TYPE);
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%d] [%s] Error registering notifications for handle 0x%2X, status=%d", this->connection_index_,
                 this->address_str_.c_str(), param->reg_for_notify.handle, param->reg_for_notify.status);
        this->proxy_->send_gatt_error(this->address_, param->reg_for_notify.handle, param->reg_for_notify.status);
        break;
      }
      api::BluetoothGATTNotifyResponse resp;
      resp.address = this->address_;
      resp.handle = param->reg_for_notify.handle;
      this->proxy_->get_api_connection()->send_message(resp, api::BluetoothGATTNotifyResponse::MESSAGE_TYPE);
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      ESP_LOGV(TAG, "[%d] [%s] ESP_GATTC_NOTIFY_EVT: handle=0x%2X", this->connection_index_, this->address_str_.c_str(),
               param->notify.handle);
      api::BluetoothGATTNotifyDataResponse resp;
      resp.address = this->address_;
      resp.handle = param->notify.handle;
      resp.set_data(param->notify.value, param->notify.value_len);
      this->proxy_->get_api_connection()->send_message(resp, api::BluetoothGATTNotifyDataResponse::MESSAGE_TYPE);
      break;
    }
    default:
      break;
  }
  return true;
}

void BluetoothConnection::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  BLEClientBase::gap_event_handler(event, param);

  switch (event) {
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
      if (memcmp(param->ble_security.auth_cmpl.bd_addr, this->remote_bda_, 6) != 0)
        break;
      if (param->ble_security.auth_cmpl.success) {
        this->proxy_->send_device_pairing(this->address_, true);
      } else {
        this->proxy_->send_device_pairing(this->address_, false, param->ble_security.auth_cmpl.fail_reason);
      }
      break;
    default:
      break;
  }
}

esp_err_t BluetoothConnection::read_characteristic(uint16_t handle) {
  if (!this->connected()) {
    ESP_LOGW(TAG, "[%d] [%s] Cannot read GATT characteristic, not connected.", this->connection_index_,
             this->address_str_.c_str());
    return ESP_GATT_NOT_CONNECTED;
  }

  ESP_LOGV(TAG, "[%d] [%s] Reading GATT characteristic handle %d", this->connection_index_, this->address_str_.c_str(),
           handle);

  esp_err_t err = esp_ble_gattc_read_char(this->gattc_if_, this->conn_id_, handle, ESP_GATT_AUTH_REQ_NONE);
  if (err != ERR_OK) {
    ESP_LOGW(TAG, "[%d] [%s] esp_ble_gattc_read_char error, err=%d", this->connection_index_,
             this->address_str_.c_str(), err);
    return err;
  }
  return ESP_OK;
}

esp_err_t BluetoothConnection::write_characteristic(uint16_t handle, const std::string &data, bool response) {
  if (!this->connected()) {
    ESP_LOGW(TAG, "[%d] [%s] Cannot write GATT characteristic, not connected.", this->connection_index_,
             this->address_str_.c_str());
    return ESP_GATT_NOT_CONNECTED;
  }
  ESP_LOGV(TAG, "[%d] [%s] Writing GATT characteristic handle %d", this->connection_index_, this->address_str_.c_str(),
           handle);

  esp_err_t err =
      esp_ble_gattc_write_char(this->gattc_if_, this->conn_id_, handle, data.size(), (uint8_t *) data.data(),
                               response ? ESP_GATT_WRITE_TYPE_RSP : ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (err != ERR_OK) {
    ESP_LOGW(TAG, "[%d] [%s] esp_ble_gattc_write_char error, err=%d", this->connection_index_,
             this->address_str_.c_str(), err);
    return err;
  }
  return ESP_OK;
}

esp_err_t BluetoothConnection::read_descriptor(uint16_t handle) {
  if (!this->connected()) {
    ESP_LOGW(TAG, "[%d] [%s] Cannot read GATT descriptor, not connected.", this->connection_index_,
             this->address_str_.c_str());
    return ESP_GATT_NOT_CONNECTED;
  }
  ESP_LOGV(TAG, "[%d] [%s] Reading GATT descriptor handle %d", this->connection_index_, this->address_str_.c_str(),
           handle);

  esp_err_t err = esp_ble_gattc_read_char_descr(this->gattc_if_, this->conn_id_, handle, ESP_GATT_AUTH_REQ_NONE);
  if (err != ERR_OK) {
    ESP_LOGW(TAG, "[%d] [%s] esp_ble_gattc_read_char_descr error, err=%d", this->connection_index_,
             this->address_str_.c_str(), err);
    return err;
  }
  return ESP_OK;
}

esp_err_t BluetoothConnection::write_descriptor(uint16_t handle, const std::string &data, bool response) {
  if (!this->connected()) {
    ESP_LOGW(TAG, "[%d] [%s] Cannot write GATT descriptor, not connected.", this->connection_index_,
             this->address_str_.c_str());
    return ESP_GATT_NOT_CONNECTED;
  }
  ESP_LOGV(TAG, "[%d] [%s] Writing GATT descriptor handle %d", this->connection_index_, this->address_str_.c_str(),
           handle);

  esp_err_t err = esp_ble_gattc_write_char_descr(
      this->gattc_if_, this->conn_id_, handle, data.size(), (uint8_t *) data.data(),
      response ? ESP_GATT_WRITE_TYPE_RSP : ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (err != ERR_OK) {
    ESP_LOGW(TAG, "[%d] [%s] esp_ble_gattc_write_char_descr error, err=%d", this->connection_index_,
             this->address_str_.c_str(), err);
    return err;
  }
  return ESP_OK;
}

esp_err_t BluetoothConnection::notify_characteristic(uint16_t handle, bool enable) {
  if (!this->connected()) {
    ESP_LOGW(TAG, "[%d] [%s] Cannot notify GATT characteristic, not connected.", this->connection_index_,
             this->address_str_.c_str());
    return ESP_GATT_NOT_CONNECTED;
  }

  if (enable) {
    ESP_LOGV(TAG, "[%d] [%s] Registering for GATT characteristic notifications handle %d", this->connection_index_,
             this->address_str_.c_str(), handle);
    esp_err_t err = esp_ble_gattc_register_for_notify(this->gattc_if_, this->remote_bda_, handle);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "[%d] [%s] esp_ble_gattc_register_for_notify failed, err=%d", this->connection_index_,
               this->address_str_.c_str(), err);
      return err;
    }
  } else {
    ESP_LOGV(TAG, "[%d] [%s] Unregistering for GATT characteristic notifications handle %d", this->connection_index_,
             this->address_str_.c_str(), handle);
    esp_err_t err = esp_ble_gattc_unregister_for_notify(this->gattc_if_, this->remote_bda_, handle);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "[%d] [%s] esp_ble_gattc_unregister_for_notify failed, err=%d", this->connection_index_,
               this->address_str_.c_str(), err);
      return err;
    }
  }
  return ESP_OK;
}

esp32_ble_tracker::AdvertisementParserType BluetoothConnection::get_advertisement_parser_type() {
  return this->proxy_->get_advertisement_parser_type();
}

}  // namespace esphome::bluetooth_proxy

#endif  // USE_ESP32
