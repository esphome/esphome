#include "bluetooth_proxy.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32

#include "esphome/components/api/api_server.h"

namespace esphome {
namespace bluetooth_proxy {

static const char *const TAG = "bluetooth_proxy";

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
    if (connection->send_service_ == connection->services_.size()) {
      connection->send_service_ = -1;
      api::global_api_server->send_bluetooth_gatt_services_done(connection->get_address());
    } else if (connection->send_service_ >= 0) {
      auto &service = connection->services_[connection->send_service_];
      api::BluetoothGATTGetServicesResponse resp;
      resp.address = connection->get_address();
      api::BluetoothGATTService service_resp;
      service_resp.uuid = {service->uuid.get_128bit_high(), service->uuid.get_128bit_low()};
      service_resp.handle = service->start_handle;
      uint16_t char_offset = 0;
      esp_gattc_char_elem_t char_result;
      while (true) {  // characteristics
        uint16_t char_count = 1;
        esp_gatt_status_t char_status =
            esp_ble_gattc_get_all_char(connection->get_gattc_if(), connection->get_conn_id(), service->start_handle,
                                       service->end_handle, &char_result, &char_count, char_offset);
        if (char_status == ESP_GATT_INVALID_OFFSET || char_status == ESP_GATT_NOT_FOUND) {
          break;
        }
        if (char_status != ESP_GATT_OK) {
          ESP_LOGW(TAG, "[%d] [%s] esp_ble_gattc_get_all_char error, status=%d", connection->get_connection_index(),
                   connection->address_str().c_str(), char_status);
          break;
        }
        if (char_count == 0) {
          break;
        }
        api::BluetoothGATTCharacteristic characteristic_resp;
        auto char_uuid = espbt::ESPBTUUID::from_uuid(char_result.uuid);
        characteristic_resp.uuid = {char_uuid.get_128bit_high(), char_uuid.get_128bit_low()};
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
            ESP_LOGW(TAG, "[%d] [%s] esp_ble_gattc_get_all_descr error, status=%d", connection->get_connection_index(),
                     connection->address_str().c_str(), desc_status);
            break;
          }
          if (desc_count == 0) {
            break;
          }
          api::BluetoothGATTDescriptor descriptor_resp;
          auto desc_uuid = espbt::ESPBTUUID::from_uuid(desc_result.uuid);
          descriptor_resp.uuid = {desc_uuid.get_128bit_high(), desc_uuid.get_128bit_low()};
          descriptor_resp.handle = desc_result.handle;
          characteristic_resp.descriptors.push_back(std::move(descriptor_resp));
          desc_offset++;
        }
        service_resp.characteristics.push_back(std::move(characteristic_resp));
      }
      resp.services.push_back(std::move(service_resp));
      api::global_api_server->send_bluetooth_gatt_services(resp);
      connection->send_service_++;
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
      } else if (connection->state() != espbt::ClientState::INIT) {
        ESP_LOGW(TAG, "[%d] [%s] Connection already in progress", connection->get_connection_index(),
                 connection->address_str().c_str());
        return;
      }
      if (msg.has_address_type) {
        connection->remote_bda_[0] = (msg.address >> 40) & 0xFF;
        connection->remote_bda_[1] = (msg.address >> 32) & 0xFF;
        connection->remote_bda_[2] = (msg.address >> 24) & 0xFF;
        connection->remote_bda_[3] = (msg.address >> 16) & 0xFF;
        connection->remote_bda_[4] = (msg.address >> 8) & 0xFF;
        connection->remote_bda_[5] = (msg.address >> 0) & 0xFF;
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
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_PAIR:
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_UNPAIR:
      break;
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
  if (connection->services_.empty()) {
    ESP_LOGW(TAG, "[%d] [%s] No GATT services found", connection->connection_index_, connection->address_str().c_str());
    api::global_api_server->send_bluetooth_gatt_services_done(msg.address);
    return;
  }
  if (connection->send_service_ == -1)  // Don't start sending services again if we're already sending them
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
