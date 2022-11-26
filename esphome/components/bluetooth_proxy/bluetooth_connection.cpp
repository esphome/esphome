#include "bluetooth_connection.h"

#include "esphome/components/api/api_server.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include "bluetooth_proxy.h"

namespace esphome {
namespace bluetooth_proxy {

static const char *const TAG = "bluetooth_proxy.connection";

bool BluetoothConnection::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                              esp_ble_gattc_cb_param_t *param) {
  if (!BLEClientBase::gattc_event_handler(event, gattc_if, param))
    return false;

  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT: {
      api::global_api_server->send_bluetooth_device_connection(this->address_, false, 0, param->disconnect.reason);
      this->set_address(0);
      api::global_api_server->send_bluetooth_connections_free(this->proxy_->get_bluetooth_connections_free(),
                                                              this->proxy_->get_bluetooth_connections_limit());
      break;
    }
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.conn_id != this->conn_id_)
        break;
      if (param->open.status != ESP_GATT_OK && param->open.status != ESP_GATT_ALREADY_OPEN) {
        api::global_api_server->send_bluetooth_device_connection(this->address_, false, 0, param->open.status);
        this->set_address(0);
        api::global_api_server->send_bluetooth_connections_free(this->proxy_->get_bluetooth_connections_free(),
                                                                this->proxy_->get_bluetooth_connections_limit());
      }
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (param->search_cmpl.conn_id != this->conn_id_)
        break;
      api::global_api_server->send_bluetooth_device_connection(this->address_, true, this->mtu_);
      api::global_api_server->send_bluetooth_connections_free(this->proxy_->get_bluetooth_connections_free(),
                                                              this->proxy_->get_bluetooth_connections_limit());
      break;
    }
    case ESP_GATTC_READ_DESCR_EVT:
    case ESP_GATTC_READ_CHAR_EVT: {
      if (param->read.conn_id != this->conn_id_)
        break;
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%d] [%s] Error reading char/descriptor at handle 0x%2X, status=%d", this->connection_index_,
                 this->address_str_.c_str(), param->read.handle, param->read.status);
        api::global_api_server->send_bluetooth_gatt_error(this->address_, param->read.handle, param->read.status);
        break;
      }
      api::BluetoothGATTReadResponse resp;
      resp.address = this->address_;
      resp.handle = param->read.handle;
      resp.data.reserve(param->read.value_len);
      for (uint16_t i = 0; i < param->read.value_len; i++) {
        resp.data.push_back(param->read.value[i]);
      }
      api::global_api_server->send_bluetooth_gatt_read_response(resp);
      break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT:
    case ESP_GATTC_WRITE_DESCR_EVT: {
      if (param->write.conn_id != this->conn_id_)
        break;
      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%d] [%s] Error writing char/descriptor at handle 0x%2X, status=%d", this->connection_index_,
                 this->address_str_.c_str(), param->write.handle, param->write.status);
        api::global_api_server->send_bluetooth_gatt_error(this->address_, param->write.handle, param->write.status);
        break;
      }
      api::BluetoothGATTWriteResponse resp;
      resp.address = this->address_;
      resp.handle = param->write.handle;
      api::global_api_server->send_bluetooth_gatt_write_response(resp);
      break;
    }
    case ESP_GATTC_UNREG_FOR_NOTIFY_EVT: {
      if (this->get_characteristic(param->unreg_for_notify.handle) == nullptr)  // No conn_id in this event
        break;
      if (param->unreg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%d] [%s] Error unregistering notifications for handle 0x%2X, status=%d",
                 this->connection_index_, this->address_str_.c_str(), param->unreg_for_notify.handle,
                 param->unreg_for_notify.status);
        api::global_api_server->send_bluetooth_gatt_error(this->address_, param->unreg_for_notify.handle,
                                                          param->unreg_for_notify.status);
        break;
      }
      api::BluetoothGATTNotifyResponse resp;
      resp.address = this->address_;
      resp.handle = param->unreg_for_notify.handle;
      api::global_api_server->send_bluetooth_gatt_notify_response(resp);
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (this->get_characteristic(param->reg_for_notify.handle) == nullptr)  // No conn_id in this event
        break;
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%d] [%s] Error registering notifications for handle 0x%2X, status=%d", this->connection_index_,
                 this->address_str_.c_str(), param->reg_for_notify.handle, param->reg_for_notify.status);
        api::global_api_server->send_bluetooth_gatt_error(this->address_, param->reg_for_notify.handle,
                                                          param->reg_for_notify.status);
        break;
      }
      api::BluetoothGATTNotifyResponse resp;
      resp.address = this->address_;
      resp.handle = param->reg_for_notify.handle;
      api::global_api_server->send_bluetooth_gatt_notify_response(resp);
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.conn_id != this->conn_id_)
        break;
      ESP_LOGV(TAG, "[%d] [%s] ESP_GATTC_NOTIFY_EVT: handle=0x%2X", this->connection_index_, this->address_str_.c_str(),
               param->notify.handle);
      api::BluetoothGATTNotifyDataResponse resp;
      resp.address = this->address_;
      resp.handle = param->notify.handle;
      resp.data.reserve(param->notify.value_len);
      for (uint16_t i = 0; i < param->notify.value_len; i++) {
        resp.data.push_back(param->notify.value[i]);
      }
      api::global_api_server->send_bluetooth_gatt_notify_data_response(resp);
      break;
    }
    default:
      break;
  }
  return true;
}

esp_err_t BluetoothConnection::read_characteristic(uint16_t handle) {
  if (!this->connected()) {
    ESP_LOGW(TAG, "[%d] [%s] Cannot read GATT characteristic, not connected.", this->connection_index_,
             this->address_str_.c_str());
    return ESP_GATT_NOT_CONNECTED;
  }
  auto *characteristic = this->get_characteristic(handle);
  if (characteristic == nullptr) {
    ESP_LOGW(TAG, "[%d] [%s] Cannot read GATT characteristic, not found.", this->connection_index_,
             this->address_str_.c_str());
    return ESP_GATT_INVALID_HANDLE;
  }

  ESP_LOGV(TAG, "[%d] [%s] Reading GATT characteristic %s", this->connection_index_, this->address_str_.c_str(),
           characteristic->uuid.to_string().c_str());

  esp_err_t err =
      esp_ble_gattc_read_char(this->gattc_if_, this->conn_id_, characteristic->handle, ESP_GATT_AUTH_REQ_NONE);
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
  auto *characteristic = this->get_characteristic(handle);
  if (characteristic == nullptr) {
    ESP_LOGW(TAG, "[%d] [%s] Cannot write GATT characteristic, not found.", this->connection_index_,
             this->address_str_.c_str());
    return ESP_GATT_INVALID_HANDLE;
  }

  ESP_LOGV(TAG, "[%d] [%s] Writing GATT characteristic %s", this->connection_index_, this->address_str_.c_str(),
           characteristic->uuid.to_string().c_str());

  auto err = characteristic->write_value((uint8_t *) data.data(), data.size(),
                                         response ? ESP_GATT_WRITE_TYPE_RSP : ESP_GATT_WRITE_TYPE_NO_RSP);
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
  auto *descriptor = this->get_descriptor(handle);
  if (descriptor == nullptr) {
    ESP_LOGW(TAG, "[%d] [%s] Cannot read GATT descriptor, not found.", this->connection_index_,
             this->address_str_.c_str());
    return ESP_GATT_INVALID_HANDLE;
  }

  ESP_LOGV(TAG, "[%d] [%s] Reading GATT descriptor %s", this->connection_index_, this->address_str_.c_str(),
           descriptor->uuid.to_string().c_str());

  esp_err_t err =
      esp_ble_gattc_read_char_descr(this->gattc_if_, this->conn_id_, descriptor->handle, ESP_GATT_AUTH_REQ_NONE);
  if (err != ERR_OK) {
    ESP_LOGW(TAG, "[%d] [%s] esp_ble_gattc_read_char_descr error, err=%d", this->connection_index_,
             this->address_str_.c_str(), err);
    return err;
  }
  return ESP_OK;
}

esp_err_t BluetoothConnection::write_descriptor(uint16_t handle, const std::string &data) {
  if (!this->connected()) {
    ESP_LOGW(TAG, "[%d] [%s] Cannot write GATT descriptor, not connected.", this->connection_index_,
             this->address_str_.c_str());
    return ESP_GATT_NOT_CONNECTED;
  }
  auto *descriptor = this->get_descriptor(handle);
  if (descriptor == nullptr) {
    ESP_LOGW(TAG, "[%d] [%s] Cannot write GATT descriptor, not found.", this->connection_index_,
             this->address_str_.c_str());
    return ESP_GATT_INVALID_HANDLE;
  }

  ESP_LOGV(TAG, "[%d] [%s] Writing GATT descriptor %s", this->connection_index_, this->address_str_.c_str(),
           descriptor->uuid.to_string().c_str());

  auto err =
      esp_ble_gattc_write_char_descr(this->gattc_if_, this->conn_id_, descriptor->handle, data.size(),
                                     (uint8_t *) data.data(), ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
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
  auto *characteristic = this->get_characteristic(handle);
  if (characteristic == nullptr) {
    ESP_LOGW(TAG, "[%d] [%s] Cannot notify GATT characteristic, not found.", this->connection_index_,
             this->address_str_.c_str());
    return ESP_GATT_INVALID_HANDLE;
  }

  if (!(characteristic->properties & (ESP_GATT_CHAR_PROP_BIT_NOTIFY | ESP_GATT_CHAR_PROP_BIT_INDICATE))) {
    ESP_LOGW(TAG, "[%d] [%s] Cannot notify GATT characteristic, does not support notify or indicate.",
             this->connection_index_, this->address_str_.c_str());
    return ESP_GATT_REQ_NOT_SUPPORTED;
  }

  if (enable) {
    ESP_LOGV(TAG, "[%d] [%s] Registering for GATT characteristic notifications %s", this->connection_index_,
             this->address_str_.c_str(), characteristic->uuid.to_string().c_str());
    esp_err_t err = esp_ble_gattc_register_for_notify(this->gattc_if_, this->remote_bda_, characteristic->handle);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "[%d] [%s] esp_ble_gattc_register_for_notify failed, err=%d", this->connection_index_,
               this->address_str_.c_str(), err);
      return err;
    }
  } else {
    ESP_LOGV(TAG, "[%d] [%s] Unregistering for GATT characteristic notifications %s", this->connection_index_,
             this->address_str_.c_str(), characteristic->uuid.to_string().c_str());
    esp_err_t err = esp_ble_gattc_unregister_for_notify(this->gattc_if_, this->remote_bda_, characteristic->handle);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "[%d] [%s] esp_ble_gattc_unregister_for_notify failed, err=%d", this->connection_index_,
               this->address_str_.c_str(), err);
      return err;
    }
  }
  return ESP_OK;
}

}  // namespace bluetooth_proxy
}  // namespace esphome

#endif  // USE_ESP32
