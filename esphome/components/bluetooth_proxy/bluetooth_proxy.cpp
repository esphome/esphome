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

  if (this->address_ == 0)
    return true;
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
void BluetoothProxy::bluetooth_gatt_read(const api::BluetoothGATTReadRequest &msg) {}
void BluetoothProxy::bluetooth_gatt_write(const api::BluetoothGATTWriteRequest &msg) {}
#endif

BluetoothProxy *global_bluetooth_proxy = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace bluetooth_proxy
}  // namespace esphome

#endif  // USE_ESP32
