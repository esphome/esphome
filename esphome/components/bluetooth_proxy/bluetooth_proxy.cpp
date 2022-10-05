#include "bluetooth_proxy.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif

namespace esphome {
namespace bluetooth_proxy {

static const char *const TAG = "bluetooth_proxy";

BluetoothProxy::BluetoothProxy() {
  global_bluetooth_proxy = this;
  this->address_ = 0;
}

bool BluetoothProxy::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  ESP_LOGV(TAG, "Proxying packet from %s - %s. RSSI: %d dB", device.get_name().c_str(), device.address_str().c_str(),
           device.get_rssi());
  this->send_api_packet_(device);

  this->address_type_map_[device.address_uint64()] = device.get_address_type();

  if (this->address_ == 0)
    return true;

  if (this->state_ == espbt::ClientState::DISCOVERED) {
    ESP_LOGV(TAG, "Connecting to address %s", this->address_str().c_str());
    return true;
  }

  BLEClientBase::parse_device(device);
  return true;
}

void BluetoothProxy::send_api_packet_(const esp32_ble_tracker::ESPBTDevice &device) {
#ifndef USE_API
  return;
#else
  api::BluetoothLEAdvertisementResponse resp;
  resp.address = device.address_uint64();
  if (!device.get_name().empty())
    resp.name = device.get_name();
  resp.rssi = device.get_rssi();
  for (auto uuid : device.get_service_uuids()) {
    resp.service_uuids.push_back(uuid.to_string());
  }
  for (auto &data : device.get_service_datas()) {
    api::BluetoothServiceData service_data;
    service_data.uuid = data.uuid.to_string();
    service_data.data.assign(data.data.begin(), data.data.end());
    resp.service_data.push_back(std::move(service_data));
  }
  for (auto &data : device.get_manufacturer_datas()) {
    api::BluetoothServiceData manufacturer_data;
    manufacturer_data.uuid = data.uuid.to_string();
    manufacturer_data.data.assign(data.data.begin(), data.data.end());
    resp.manufacturer_data.push_back(std::move(manufacturer_data));
  }
  api::global_api_server->send_bluetooth_le_advertisement(resp);
#endif
}

void BluetoothProxy::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                         esp_ble_gattc_cb_param_t *param) {
  BLEClientBase::gattc_event_handler(event, gattc_if, param);
  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT: {
#ifdef USE_API
      api::global_api_server->send_bluetooth_device_connection(this->address_, false, this->mtu_,
                                                               param->disconnect.reason);
      api::global_api_server->send_bluetooth_connections_free(this->get_bluetooth_connections_free(),
                                                              this->get_bluetooth_connections_limit());
#endif
      this->address_ = 0;
    }
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status != ESP_GATT_OK && param->open.status != ESP_GATT_ALREADY_OPEN) {
#ifdef USE_API
        api::global_api_server->send_bluetooth_device_connection(this->address_, false, this->mtu_, param->open.status);

#endif
        break;
      }
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
#ifdef USE_API
      api::global_api_server->send_bluetooth_device_connection(this->address_, true, this->mtu_);
      api::global_api_server->send_bluetooth_connections_free(this->get_bluetooth_connections_free(),
                                                              this->get_bluetooth_connections_limit());
#endif
      break;
    }
    case ESP_GATTC_READ_DESCR_EVT:
    case ESP_GATTC_READ_CHAR_EVT: {
      if (param->read.conn_id != this->conn_id_)
        break;
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Error reading char/descriptor at handle %d, status=%d", param->read.handle, param->read.status);
        break;
      }
#ifdef USE_API
      api::BluetoothGATTReadResponse resp;
      resp.address = this->address_;
      resp.handle = param->read.handle;
      resp.data.reserve(param->read.value_len);
      for (uint16_t i = 0; i < param->read.value_len; i++) {
        resp.data.push_back(param->read.value[i]);
      }
      api::global_api_server->send_bluetooth_gatt_read_response(resp);
#endif
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.conn_id != this->conn_id_)
        break;
      ESP_LOGV(TAG, "ESP_GATTC_NOTIFY_EVT: handle=0x%x", param->notify.handle);
#ifdef USE_API
      api::BluetoothGATTNotifyDataResponse resp;
      resp.address = this->address_;
      resp.handle = param->notify.handle;
      resp.data.reserve(param->notify.value_len);
      for (uint16_t i = 0; i < param->notify.value_len; i++) {
        resp.data.push_back(param->notify.value[i]);
      }
      api::global_api_server->send_bluetooth_gatt_notify_data_response(resp);
#endif
      break;
    }
    default:
      break;
  }
}

void BluetoothProxy::dump_config() { ESP_LOGCONFIG(TAG, "Bluetooth Proxy:"); }

void BluetoothProxy::loop() {
  BLEClientBase::loop();
#ifdef USE_API
  if (this->state_ != espbt::ClientState::IDLE && !api::global_api_server->is_connected()) {
    ESP_LOGI(TAG, "[%s] Disconnecting.", this->address_str().c_str());
    auto err = esp_ble_gattc_close(this->gattc_if_, this->conn_id_);
    if (err != ERR_OK) {
      ESP_LOGW(TAG, "esp_ble_gattc_close error, address=%s err=%d", this->address_str().c_str(), err);
    }
  }

  if (this->send_service_ == this->services_.size()) {
    this->send_service_ = -1;
    api::global_api_server->send_bluetooth_gatt_services_done(this->address_);
  } else if (this->send_service_ >= 0) {
    auto &service = this->services_[this->send_service_];
    api::BluetoothGATTGetServicesResponse resp;
    resp.address = this->address_;
    api::BluetoothGATTService service_resp;
    service_resp.uuid = {service->uuid.get_128bit_high(), service->uuid.get_128bit_low()};
    service_resp.handle = service->start_handle;
    for (auto &characteristic : service->characteristics) {
      api::BluetoothGATTCharacteristic characteristic_resp;
      characteristic_resp.uuid = {characteristic->uuid.get_128bit_high(), characteristic->uuid.get_128bit_low()};
      characteristic_resp.handle = characteristic->handle;
      characteristic_resp.properties = characteristic->properties;
      for (auto &descriptor : characteristic->descriptors) {
        api::BluetoothGATTDescriptor descriptor_resp;
        descriptor_resp.uuid = {descriptor->uuid.get_128bit_high(), descriptor->uuid.get_128bit_low()};
        descriptor_resp.handle = descriptor->handle;
        characteristic_resp.descriptors.push_back(std::move(descriptor_resp));
      }
      service_resp.characteristics.push_back(std::move(characteristic_resp));
    }
    resp.services.push_back(std::move(service_resp));
    api::global_api_server->send_bluetooth_gatt_services(resp);
    this->send_service_++;
  }
#endif
}

#ifdef USE_API
void BluetoothProxy::bluetooth_device_request(const api::BluetoothDeviceRequest &msg) {
  switch (msg.request_type) {
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT: {
      this->address_ = msg.address;
      if (this->address_type_map_.find(this->address_) != this->address_type_map_.end()) {
        // Utilise the address type cache
        this->remote_addr_type_ = this->address_type_map_[this->address_];
      } else {
        this->remote_addr_type_ = BLE_ADDR_TYPE_PUBLIC;
      }
      this->remote_bda_[0] = (this->address_ >> 40) & 0xFF;
      this->remote_bda_[1] = (this->address_ >> 32) & 0xFF;
      this->remote_bda_[2] = (this->address_ >> 24) & 0xFF;
      this->remote_bda_[3] = (this->address_ >> 16) & 0xFF;
      this->remote_bda_[4] = (this->address_ >> 8) & 0xFF;
      this->remote_bda_[5] = (this->address_ >> 0) & 0xFF;
      this->set_state(espbt::ClientState::DISCOVERED);
      esp_ble_gap_stop_scanning();
      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_DISCONNECT: {
      if (this->state() != espbt::ClientState::IDLE) {
        ESP_LOGI(TAG, "[%s] Disconnecting.", this->address_str().c_str());
        auto err = esp_ble_gattc_close(this->gattc_if_, this->conn_id_);
        if (err != ERR_OK) {
          ESP_LOGW(TAG, "esp_ble_gattc_close error, address=%s err=%d", this->address_str().c_str(), err);
        }
      }
      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_PAIR:
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_UNPAIR:
      break;
  }
}

void BluetoothProxy::bluetooth_gatt_read(const api::BluetoothGATTReadRequest &msg) {
  if (this->state_ != espbt::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "Cannot read GATT characteristic, not connected.");
    return;
  }
  if (this->address_ != msg.address) {
    ESP_LOGW(TAG, "Address mismatch for read GATT characteristic request");
    return;
  }

  auto *characteristic = this->get_characteristic(msg.handle);
  if (characteristic == nullptr) {
    ESP_LOGW(TAG, "Cannot read GATT characteristic, not found.");
    return;
  }

  ESP_LOGV(TAG, "Reading GATT characteristic %s", characteristic->uuid.to_string().c_str());

  esp_err_t err =
      esp_ble_gattc_read_char(this->gattc_if_, this->conn_id_, characteristic->handle, ESP_GATT_AUTH_REQ_NONE);
  if (err != ERR_OK) {
    ESP_LOGW(TAG, "esp_ble_gattc_read_char error, err=%d", err);
  }
}

void BluetoothProxy::bluetooth_gatt_write(const api::BluetoothGATTWriteRequest &msg) {
  if (this->state_ != espbt::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "Cannot write GATT characteristic, not connected.");
    return;
  }
  if (this->address_ != msg.address) {
    ESP_LOGW(TAG, "Address mismatch for write GATT characteristic request");
    return;
  }

  auto *characteristic = this->get_characteristic(msg.handle);
  if (characteristic == nullptr) {
    ESP_LOGW(TAG, "Cannot write GATT characteristic, not found.");
    return;
  }

  ESP_LOGV(TAG, "Writing GATT characteristic %s", characteristic->uuid.to_string().c_str());
  characteristic->write_value((uint8_t *) msg.data.data(), msg.data.size(),
                              msg.response ? ESP_GATT_WRITE_TYPE_RSP : ESP_GATT_WRITE_TYPE_NO_RSP);
}

void BluetoothProxy::bluetooth_gatt_read_descriptor(const api::BluetoothGATTReadDescriptorRequest &msg) {
  if (this->state_ != espbt::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "Cannot read GATT characteristic descriptor, not connected.");
    return;
  }
  if (this->address_ != msg.address) {
    ESP_LOGW(TAG, "Address mismatch for read GATT characteristic descriptor request");
    return;
  }

  auto *descriptor = this->get_descriptor(msg.handle);
  if (descriptor == nullptr) {
    ESP_LOGW(TAG, "Cannot read GATT characteristic descriptor, not found.");
    return;
  }

  ESP_LOGV(TAG, "Reading GATT characteristic descriptor %s -> %s", descriptor->characteristic->uuid.to_string().c_str(),
           descriptor->uuid.to_string().c_str());

  esp_err_t err =
      esp_ble_gattc_read_char_descr(this->gattc_if_, this->conn_id_, descriptor->handle, ESP_GATT_AUTH_REQ_NONE);
  if (err != ERR_OK) {
    ESP_LOGW(TAG, "esp_ble_gattc_read_char error, err=%d", err);
  }
}

void BluetoothProxy::bluetooth_gatt_write_descriptor(const api::BluetoothGATTWriteDescriptorRequest &msg) {
  if (this->state_ != espbt::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "Cannot write GATT characteristic descriptor, not connected.");
    return;
  }
  if (this->address_ != msg.address) {
    ESP_LOGW(TAG, "Address mismatch for write GATT characteristic descriptor request");
    return;
  }

  auto *descriptor = this->get_descriptor(msg.handle);
  if (descriptor == nullptr) {
    ESP_LOGW(TAG, "Cannot write GATT characteristic descriptor, not found.");
    return;
  }

  ESP_LOGV(TAG, "Writing GATT characteristic descriptor %s -> %s", descriptor->characteristic->uuid.to_string().c_str(),
           descriptor->uuid.to_string().c_str());

  esp_err_t err =
      esp_ble_gattc_write_char_descr(this->gattc_if_, this->conn_id_, descriptor->handle, msg.data.size(),
                                     (uint8_t *) msg.data.data(), ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (err != ERR_OK) {
    ESP_LOGW(TAG, "esp_ble_gattc_write_char_descr error, err=%d", err);
  }
}

void BluetoothProxy::bluetooth_gatt_send_services(const api::BluetoothGATTGetServicesRequest &msg) {
  if (this->address_ != msg.address) {
    ESP_LOGW(TAG, "Address mismatch for service list request");
    return;
  }
  this->send_service_ = 0;
}

void BluetoothProxy::bluetooth_gatt_notify(const api::BluetoothGATTNotifyRequest &msg) {
  if (this->address_ != msg.address) {
    ESP_LOGW(TAG, "Address mismatch for notify");
    return;
  }

  auto *characteristic = this->get_characteristic(msg.handle);

  if (characteristic == nullptr) {
    ESP_LOGW(TAG, "Cannot notify GATT characteristic, not found.");
    return;
  }

  esp_err_t err;
  if (msg.enable) {
    err = esp_ble_gattc_register_for_notify(this->gattc_if_, this->remote_bda_, characteristic->handle);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed, err=%d", err);
    }
  } else {
    err = esp_ble_gattc_unregister_for_notify(this->gattc_if_, this->remote_bda_, characteristic->handle);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "esp_ble_gattc_unregister_for_notify failed, err=%d", err);
    }
  }
}

#endif

BluetoothProxy *global_bluetooth_proxy = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace bluetooth_proxy
}  // namespace esphome

#endif  // USE_ESP32
