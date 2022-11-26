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
      ESP_LOGV(TAG, "[%d] [%s] Searching to connect", connection->get_connection_index(),
               connection->address_str().c_str());
      connection->set_state(espbt::ClientState::SEARCHING);
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

  auto err = connection->write_descriptor(msg.handle, msg.data);
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
