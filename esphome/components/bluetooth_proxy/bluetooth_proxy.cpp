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
    ESP_LOGD(TAG, "Connecting to address %s", this->address_str().c_str());
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
    for (auto d : data.data)
      service_data.data.push_back(d);
    resp.service_data.push_back(service_data);
  }
  for (auto &data : device.get_manufacturer_datas()) {
    api::BluetoothServiceData manufacturer_data;
    manufacturer_data.uuid = data.uuid.to_string();
    for (auto d : data.data)
      manufacturer_data.data.push_back(d);
    resp.manufacturer_data.push_back(manufacturer_data);
  }
  api::global_api_server->send_bluetooth_le_advertisement(resp);
#endif
}

void BluetoothProxy::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                         esp_ble_gattc_cb_param_t *param) {
  BLEClientBase::gattc_event_handler(event, gattc_if, param);
  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT: {
      this->address_ = 0;
    }
    case ESP_GATTC_READ_CHAR_EVT: {
      if (param->read.conn_id != this->conn_id_)
        break;
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Error reading char at handle %d, status=%d", param->read.handle, param->read.status);
        break;
      }
      BLECharacteristic *characteristic = nullptr;
      for (auto &svc : this->services_) {
        for (auto &chr : svc->characteristics) {
          if (chr->handle == param->read.handle) {
            characteristic = chr;
            break;
          }
        }
        if (characteristic != nullptr)
          break;
      }
      if (characteristic == nullptr) {
        ESP_LOGW(TAG, "Got read response for unknown handle %d", param->read.handle);
        break;
      }
#ifdef USE_API
      api::BluetoothGATTReadResponse resp;
      resp.address = this->address_;
      resp.characteristic_uuid = characteristic->uuid.to_string();
      resp.service_uuid = characteristic->service->uuid.to_string();
      for (uint16_t i = 0; i < param->read.value_len; i++) {
        resp.data.push_back(param->read.value[i]);
      }
      api::global_api_server->send_bluetooth_gatt_read_response(resp);
#endif
      break;
    }
    default:
      break;
  }
}

void BluetoothProxy::dump_config() { ESP_LOGCONFIG(TAG, "Bluetooth Proxy:"); }

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
        auto ret = esp_ble_gattc_close(this->gattc_if_, this->conn_id_);
        if (ret) {
          ESP_LOGW(TAG, "esp_ble_gattc_close error, address=%s status=%d", this->address_str().c_str(), ret);
        }
      }
      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_PAIR:
      break;
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
  ESP_LOGD(TAG, "Reading GATT characteristic %s", msg.characteristic_uuid.c_str());

  auto *characteristic = this->get_characteristic(espbt::ESPBTUUID::from_raw(msg.service_uuid),
                                                  espbt::ESPBTUUID::from_raw(msg.characteristic_uuid));
  if (characteristic == nullptr) {
    ESP_LOGW(TAG, "Cannot read GATT characteristic, not found.");
    return;
  }
  esp_err_t ret =
      esp_ble_gattc_read_char(this->gattc_if_, this->conn_id_, characteristic->handle, ESP_GATT_AUTH_REQ_NONE);
  if (ret) {
    ESP_LOGW(TAG, "esp_ble_gattc_read_char error, status=%d", ret);
  }
}

void BluetoothProxy::bluetooth_gatt_write(const api::BluetoothGATTWriteRequest &msg) {
  if (this->state_ != espbt::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "Cannot read GATT characteristic, not connected.");
    return;
  }
  if (this->address_ != msg.address) {
    ESP_LOGW(TAG, "Address mismatch for read GATT characteristic request");
    return;
  }
  ESP_LOGD(TAG, "Reading GATT characteristic %s", msg.characteristic_uuid.c_str());

  auto *characteristic = this->get_characteristic(espbt::ESPBTUUID::from_raw(msg.service_uuid),
                                                  espbt::ESPBTUUID::from_raw(msg.characteristic_uuid));
  if (characteristic == nullptr) {
    ESP_LOGW(TAG, "Cannot read GATT characteristic, not found.");
    return;
  }
  characteristic->write_value(msg.data.data(), msg.data.size());
}

api::BluetoothGATTGetServicesResponse BluetoothProxy::bluetooth_gatt_get_services(
    const api::BluetoothGATTGetServicesRequest &msg) {
  if (this->address_ != msg.address) {
    ESP_LOGW(TAG, "Address mismatch for service list request");
    return {};
  }
  api::BluetoothGATTGetServicesResponse resp;
  resp.address = msg.address;
  for (BLEService *service : this->services_) {
    api::BluetoothGATTService service_resp;
    service_resp.uuid = service->uuid.to_string();
    service_resp.handle = service->start_handle;
    for (BLECharacteristic *characteristic : service->characteristics) {
      api::BluetoothGATTCharacteristic characteristic_resp;
      characteristic_resp.uuid = characteristic->uuid.to_string();
      characteristic_resp.handle = characteristic->handle;
      characteristic_resp.properties = characteristic->properties;
      for (BLEDescriptor *descriptor : characteristic->descriptors) {
        api::BluetoothGATTDescriptor descriptor_resp;
        descriptor_resp.uuid = descriptor->uuid.to_string();
        descriptor_resp.handle = descriptor->handle;
        characteristic_resp.descriptors.push_back(descriptor_resp);
      }
      service_resp.characteristics.push_back(characteristic_resp);
    }
    resp.services.push_back(service_resp);
  }
  return resp;
}

#endif

BluetoothProxy *global_bluetooth_proxy = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace bluetooth_proxy
}  // namespace esphome

#endif  // USE_ESP32
