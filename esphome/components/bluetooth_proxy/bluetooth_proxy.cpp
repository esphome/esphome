#include "bluetooth_proxy.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32

#include "esphome/components/api/api_server.h"

namespace esphome {
namespace bluetooth_proxy {

static const char *const TAG = "bluetooth_proxy";
static const int DONE_SENDING_SERVICES = -2;

std::vector<uint64_t> get_128bit_uuid_vec(esp_bt_uuid_t uuid_source) {
  esp_bt_uuid_t uuid = espbt::ESPBTUUID::from_uuid(uuid_source).as_128bit().get_uuid();
  return std::vector<uint64_t>{((uint64_t) uuid.uuid.uuid128[15] << 56) | ((uint64_t) uuid.uuid.uuid128[14] << 48) |
                                   ((uint64_t) uuid.uuid.uuid128[13] << 40) | ((uint64_t) uuid.uuid.uuid128[12] << 32) |
                                   ((uint64_t) uuid.uuid.uuid128[11] << 24) | ((uint64_t) uuid.uuid.uuid128[10] << 16) |
                                   ((uint64_t) uuid.uuid.uuid128[9] << 8) | ((uint64_t) uuid.uuid.uuid128[8]),
                               ((uint64_t) uuid.uuid.uuid128[7] << 56) | ((uint64_t) uuid.uuid.uuid128[6] << 48) |
                                   ((uint64_t) uuid.uuid.uuid128[5] << 40) | ((uint64_t) uuid.uuid.uuid128[4] << 32) |
                                   ((uint64_t) uuid.uuid.uuid128[3] << 24) | ((uint64_t) uuid.uuid.uuid128[2] << 16) |
                                   ((uint64_t) uuid.uuid.uuid128[1] << 8) | ((uint64_t) uuid.uuid.uuid128[0])};
}

BluetoothProxy::BluetoothProxy() { global_bluetooth_proxy = this; }

bool BluetoothProxy::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (!api::global_api_server->is_connected())
    return false;
  ESP_LOGV(TAG, "Proxying packet from %s - %s. RSSI: %d dB", device.get_name().c_str(), device.address_str().c_str(),
           device.get_rssi());
  this->send_api_packet_(device);

  return true;
}

void BluetoothProxy::send_api_packet_(const esp32_ble_tracker::ESPBTDevice &device) {
  api::BluetoothLEAdvertisementResponse resp;
  resp.address = device.address_uint64();
  resp.address_type = device.get_address_type();
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
}

void BluetoothProxy::dump_config() {
  ESP_LOGCONFIG(TAG, "Bluetooth Proxy:");
  ESP_LOGCONFIG(TAG, "  Active: %s", YESNO(this->active_));
}

int BluetoothProxy::get_bluetooth_connections_free() {
  int free = 0;
  for (auto *connection : this->connections_) {
    if (connection->address_ == 0) {
      free++;
      ESP_LOGV(TAG, "[%d] Free connection", connection->get_connection_index());
    } else {
      ESP_LOGV(TAG, "[%d] Used connection by [%s]", connection->get_connection_index(),
               connection->address_str().c_str());
    }
  }
  return free;
}

void BluetoothProxy::loop() {
  if (!api::global_api_server->is_connected()) {
    for (auto *connection : this->connections_) {
      if (connection->get_address() != 0) {
        connection->disconnect();
      }
    }
    return;
  }
  for (auto *connection : this->connections_) {
    if (connection->send_service_ == connection->service_count_) {
      connection->send_service_ = DONE_SENDING_SERVICES;
      api::global_api_server->send_bluetooth_gatt_services_done(connection->get_address());
      if (connection->connection_type_ == espbt::ConnectionType::V3_WITH_CACHE ||
          connection->connection_type_ == espbt::ConnectionType::V3_WITHOUT_CACHE) {
        connection->release_services();
      }
    } else if (connection->send_service_ >= 0) {
      esp_gattc_service_elem_t service_result;
      uint16_t service_count = 1;
      esp_gatt_status_t service_status =
          esp_ble_gattc_get_service(connection->get_gattc_if(), connection->get_conn_id(), nullptr, &service_result,
                                    &service_count, connection->send_service_);
      connection->send_service_++;
      if (service_status != ESP_GATT_OK) {
        ESP_LOGE(TAG, "[%d] [%s] esp_ble_gattc_get_service error at offset=%d, status=%d",
                 connection->get_connection_index(), connection->address_str().c_str(), connection->send_service_ - 1,
                 service_status);
        continue;
      }
      if (service_count == 0) {
        ESP_LOGE(TAG, "[%d] [%s] esp_ble_gattc_get_service missing, service_count=%d",
                 connection->get_connection_index(), connection->address_str().c_str(), service_count);
        continue;
      }
      api::BluetoothGATTGetServicesResponse resp;
      resp.address = connection->get_address();
      api::BluetoothGATTService service_resp;
      service_resp.uuid = get_128bit_uuid_vec(service_result.uuid);
      service_resp.handle = service_result.start_handle;
      uint16_t char_offset = 0;
      esp_gattc_char_elem_t char_result;
      while (true) {  // characteristics
        uint16_t char_count = 1;
        esp_gatt_status_t char_status = esp_ble_gattc_get_all_char(
            connection->get_gattc_if(), connection->get_conn_id(), service_result.start_handle,
            service_result.end_handle, &char_result, &char_count, char_offset);
        if (char_status == ESP_GATT_INVALID_OFFSET || char_status == ESP_GATT_NOT_FOUND) {
          break;
        }
        if (char_status != ESP_GATT_OK) {
          ESP_LOGE(TAG, "[%d] [%s] esp_ble_gattc_get_all_char error, status=%d", connection->get_connection_index(),
                   connection->address_str().c_str(), char_status);
          break;
        }
        if (char_count == 0) {
          break;
        }
        api::BluetoothGATTCharacteristic characteristic_resp;
        characteristic_resp.uuid = get_128bit_uuid_vec(char_result.uuid);
        characteristic_resp.handle = char_result.char_handle;
        characteristic_resp.properties = char_result.properties;
        char_offset++;
        uint16_t desc_offset = 0;
        esp_gattc_descr_elem_t desc_result;
        while (true) {  // descriptors
          uint16_t desc_count = 1;
          esp_gatt_status_t desc_status =
              esp_ble_gattc_get_all_descr(connection->get_gattc_if(), connection->get_conn_id(),
                                          char_result.char_handle, &desc_result, &desc_count, desc_offset);
          if (desc_status == ESP_GATT_INVALID_OFFSET || desc_status == ESP_GATT_NOT_FOUND) {
            break;
          }
          if (desc_status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "[%d] [%s] esp_ble_gattc_get_all_descr error, status=%d", connection->get_connection_index(),
                     connection->address_str().c_str(), desc_status);
            break;
          }
          if (desc_count == 0) {
            break;
          }
          api::BluetoothGATTDescriptor descriptor_resp;
          descriptor_resp.uuid = get_128bit_uuid_vec(desc_result.uuid);
          descriptor_resp.handle = desc_result.handle;
          characteristic_resp.descriptors.push_back(std::move(descriptor_resp));
          desc_offset++;
        }
        service_resp.characteristics.push_back(std::move(characteristic_resp));
      }
      resp.services.push_back(std::move(service_resp));
      api::global_api_server->send_bluetooth_gatt_services(resp);
    }
  }
}

BluetoothConnection *BluetoothProxy::get_connection_(uint64_t address, bool reserve) {
  for (auto *connection : this->connections_) {
    if (connection->get_address() == address)
      return connection;
  }

  if (!reserve)
    return nullptr;

  for (auto *connection : this->connections_) {
    if (connection->get_address() == 0) {
      connection->send_service_ = DONE_SENDING_SERVICES;
      connection->set_address(address);
      // All connections must start at INIT
      // We only set the state if we allocate the connection
      // to avoid a race where multiple connection attempts
      // are made.
      connection->set_state(espbt::ClientState::INIT);
      return connection;
    }
  }

  return nullptr;
}

void BluetoothProxy::bluetooth_device_request(const api::BluetoothDeviceRequest &msg) {
  switch (msg.request_type) {
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITH_CACHE:
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITHOUT_CACHE:
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT: {
      auto *connection = this->get_connection_(msg.address, true);
      if (connection == nullptr) {
        ESP_LOGW(TAG, "No free connections available");
        api::global_api_server->send_bluetooth_device_connection(msg.address, false);
        return;
      }
      if (connection->state() == espbt::ClientState::CONNECTED ||
          connection->state() == espbt::ClientState::ESTABLISHED) {
        ESP_LOGW(TAG, "[%d] [%s] Connection already established", connection->get_connection_index(),
                 connection->address_str().c_str());
        api::global_api_server->send_bluetooth_device_connection(msg.address, true);
        api::global_api_server->send_bluetooth_connections_free(this->get_bluetooth_connections_free(),
                                                                this->get_bluetooth_connections_limit());
        return;
      } else if (connection->state() == espbt::ClientState::SEARCHING) {
        ESP_LOGW(TAG, "[%d] [%s] Connection request ignored, already searching for device",
                 connection->get_connection_index(), connection->address_str().c_str());
        return;
      } else if (connection->state() == espbt::ClientState::DISCOVERED) {
        ESP_LOGW(TAG, "[%d] [%s] Connection request ignored, device already discovered",
                 connection->get_connection_index(), connection->address_str().c_str());
        return;
      } else if (connection->state() == espbt::ClientState::READY_TO_CONNECT) {
        ESP_LOGW(TAG, "[%d] [%s] Connection request ignored, waiting in line to connect",
                 connection->get_connection_index(), connection->address_str().c_str());
        return;
      } else if (connection->state() == espbt::ClientState::CONNECTING) {
        ESP_LOGW(TAG, "[%d] [%s] Connection request ignored, already connecting", connection->get_connection_index(),
                 connection->address_str().c_str());
        return;
      } else if (connection->state() == espbt::ClientState::DISCONNECTING) {
        ESP_LOGW(TAG, "[%d] [%s] Connection request ignored, device is disconnecting",
                 connection->get_connection_index(), connection->address_str().c_str());
        return;
      } else if (connection->state() != espbt::ClientState::INIT) {
        ESP_LOGW(TAG, "[%d] [%s] Connection already in progress", connection->get_connection_index(),
                 connection->address_str().c_str());
        return;
      }
      if (msg.request_type == api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITH_CACHE) {
        connection->set_connection_type(espbt::ConnectionType::V3_WITH_CACHE);
        ESP_LOGI(TAG, "[%d] [%s] Connecting v3 with cache", connection->get_connection_index(),
                 connection->address_str().c_str());
      } else if (msg.request_type == api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITHOUT_CACHE) {
        connection->set_connection_type(espbt::ConnectionType::V3_WITHOUT_CACHE);
        ESP_LOGI(TAG, "[%d] [%s] Connecting v3 without cache", connection->get_connection_index(),
                 connection->address_str().c_str());
      } else {
        connection->set_connection_type(espbt::ConnectionType::V1);
        ESP_LOGI(TAG, "[%d] [%s] Connecting v1", connection->get_connection_index(), connection->address_str().c_str());
      }
      if (msg.has_address_type) {
        uint64_to_bd_addr(msg.address, connection->remote_bda_);
        connection->set_remote_addr_type(static_cast<esp_ble_addr_type_t>(msg.address_type));
        connection->set_state(espbt::ClientState::DISCOVERED);
      } else {
        connection->set_state(espbt::ClientState::SEARCHING);
      }
      api::global_api_server->send_bluetooth_connections_free(this->get_bluetooth_connections_free(),
                                                              this->get_bluetooth_connections_limit());
      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_DISCONNECT: {
      auto *connection = this->get_connection_(msg.address, false);
      if (connection == nullptr) {
        api::global_api_server->send_bluetooth_device_connection(msg.address, false);
        api::global_api_server->send_bluetooth_connections_free(this->get_bluetooth_connections_free(),
                                                                this->get_bluetooth_connections_limit());
        return;
      }
      if (connection->state() != espbt::ClientState::IDLE) {
        connection->disconnect();
      } else {
        connection->set_address(0);
        api::global_api_server->send_bluetooth_device_connection(msg.address, false);
        api::global_api_server->send_bluetooth_connections_free(this->get_bluetooth_connections_free(),
                                                                this->get_bluetooth_connections_limit());
      }
      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_PAIR: {
      auto *connection = this->get_connection_(msg.address, false);
      if (connection != nullptr) {
        if (!connection->is_paired()) {
          auto err = connection->pair();
          if (err != ESP_OK) {
            api::global_api_server->send_bluetooth_device_pairing(msg.address, false, err);
          }
        } else {
          api::global_api_server->send_bluetooth_device_pairing(msg.address, true);
        }
      }
      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_UNPAIR: {
      esp_bd_addr_t address;
      uint64_to_bd_addr(msg.address, address);
      esp_err_t ret = esp_ble_remove_bond_device(address);
      api::global_api_server->send_bluetooth_device_unpairing(msg.address, ret == ESP_OK, ret);
      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CLEAR_CACHE: {
      esp_bd_addr_t address;
      uint64_to_bd_addr(msg.address, address);
      esp_err_t ret = esp_ble_gattc_cache_clean(address);
      api::global_api_server->send_bluetooth_device_clear_cache(msg.address, ret == ESP_OK, ret);
      break;
    }
  }
}

void BluetoothProxy::bluetooth_gatt_read(const api::BluetoothGATTReadRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr) {
    ESP_LOGW(TAG, "Cannot read GATT characteristic, not connected");
    api::global_api_server->send_bluetooth_gatt_error(msg.address, msg.handle, ESP_GATT_NOT_CONNECTED);
    return;
  }

  auto err = connection->read_characteristic(msg.handle);
  if (err != ESP_OK) {
    api::global_api_server->send_bluetooth_gatt_error(msg.address, msg.handle, err);
  }
}

void BluetoothProxy::bluetooth_gatt_write(const api::BluetoothGATTWriteRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr) {
    ESP_LOGW(TAG, "Cannot write GATT characteristic, not connected");
    api::global_api_server->send_bluetooth_gatt_error(msg.address, msg.handle, ESP_GATT_NOT_CONNECTED);
    return;
  }

  auto err = connection->write_characteristic(msg.handle, msg.data, msg.response);
  if (err != ESP_OK) {
    api::global_api_server->send_bluetooth_gatt_error(msg.address, msg.handle, err);
  }
}

void BluetoothProxy::bluetooth_gatt_read_descriptor(const api::BluetoothGATTReadDescriptorRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr) {
    ESP_LOGW(TAG, "Cannot read GATT descriptor, not connected");
    api::global_api_server->send_bluetooth_gatt_error(msg.address, msg.handle, ESP_GATT_NOT_CONNECTED);
    return;
  }

  auto err = connection->read_descriptor(msg.handle);
  if (err != ESP_OK) {
    api::global_api_server->send_bluetooth_gatt_error(msg.address, msg.handle, err);
  }
}

void BluetoothProxy::bluetooth_gatt_write_descriptor(const api::BluetoothGATTWriteDescriptorRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr) {
    ESP_LOGW(TAG, "Cannot write GATT descriptor, not connected");
    api::global_api_server->send_bluetooth_gatt_error(msg.address, msg.handle, ESP_GATT_NOT_CONNECTED);
    return;
  }

  auto err = connection->write_descriptor(msg.handle, msg.data, true);
  if (err != ESP_OK) {
    api::global_api_server->send_bluetooth_gatt_error(msg.address, msg.handle, err);
  }
}

void BluetoothProxy::bluetooth_gatt_send_services(const api::BluetoothGATTGetServicesRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr || !connection->connected()) {
    ESP_LOGW(TAG, "Cannot get GATT services, not connected");
    api::global_api_server->send_bluetooth_gatt_error(msg.address, 0, ESP_GATT_NOT_CONNECTED);
    return;
  }
  if (!connection->service_count_) {
    ESP_LOGW(TAG, "[%d] [%s] No GATT services found", connection->connection_index_, connection->address_str().c_str());
    api::global_api_server->send_bluetooth_gatt_services_done(msg.address);
    return;
  }
  if (connection->send_service_ ==
      DONE_SENDING_SERVICES)  // Only start sending services if we're not already sending them
    connection->send_service_ = 0;
}

void BluetoothProxy::bluetooth_gatt_notify(const api::BluetoothGATTNotifyRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr) {
    ESP_LOGW(TAG, "Cannot notify GATT characteristic, not connected");
    api::global_api_server->send_bluetooth_gatt_error(msg.address, msg.handle, ESP_GATT_NOT_CONNECTED);
    return;
  }

  auto err = connection->notify_characteristic(msg.handle, msg.enable);
  if (err != ESP_OK) {
    api::global_api_server->send_bluetooth_gatt_error(msg.address, msg.handle, err);
  }
}

BluetoothProxy *global_bluetooth_proxy = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace bluetooth_proxy
}  // namespace esphome

#endif  // USE_ESP32
