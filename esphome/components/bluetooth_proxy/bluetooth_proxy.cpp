#include "bluetooth_proxy.h"

#ifdef USE_API
#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_server.h"
#endif  // USE_API
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace bluetooth_proxy {

static const char *const TAG = "bluetooth_proxy";

void BluetoothProxy::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Bluetooth Proxy...");
#ifdef USE_API
  api::global_api_server->request_bluetooth_address_ignore_list([this](const std::vector<uint64_t> &addresses) {
    for (const auto &address : addresses) {
#ifdef ESPHOME_LOG_HAS_VERBOSE
      esp_bd_addr_t addr;
      addr[0] = (address >> 40) & 0xFF;
      addr[1] = (address >> 32) & 0xFF;
      addr[2] = (address >> 24) & 0xFF;
      addr[3] = (address >> 16) & 0xFF;
      addr[4] = (address >> 8) & 0xFF;
      addr[5] = (address >> 0) & 0xFF;

      ESP_LOGV(TAG, "Adding %02X:%02X:%02X:%02X:%02X:%02X to ignore list", addr[0], addr[1], addr[2], addr[3], addr[4],
               addr[5]);
#endif
      if (std::find(this->ignore_list_.begin(), this->ignore_list_.end(), address) == this->ignore_list_.end()) {
        this->ignore_list_.push_back(address);
      }
    }
  });
#endif
}

bool BluetoothProxy::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  uint64_t addr64 = device.address_uint64();
  if (this->ignore_list_.size() > 0) {
    if (std::find(this->ignore_list_.begin(), this->ignore_list_.end(), addr64) != this->ignore_list_.end()) {
      ESP_LOGV(TAG, "Ignoring packet from %s - %s. RSSI: %d dB", device.get_name().c_str(),
               device.address_str().c_str(), device.get_rssi());
      return false;
    }
  }
  ESP_LOGV(TAG, "Proxying packet from %s - %s. RSSI: %d dB", device.get_name().c_str(), device.address_str().c_str(),
           device.get_rssi());
  this->send_api_packet_(device);
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

void BluetoothProxy::dump_config() { ESP_LOGCONFIG(TAG, "Bluetooth Proxy:"); }

}  // namespace bluetooth_proxy
}  // namespace esphome

#endif  // USE_ESP32
