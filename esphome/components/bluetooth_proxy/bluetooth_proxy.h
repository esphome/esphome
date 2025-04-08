#pragma once

#ifdef USE_ESP32

#include <map>
#include <vector>

#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_pb2.h"
#include "esphome/components/esp32_ble_client/ble_client_base.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#include "bluetooth_connection.h"

#include <esp_bt.h>
#include <esp_bt_device.h>

namespace esphome {
namespace bluetooth_proxy {

static const esp_err_t ESP_GATT_NOT_CONNECTED = -1;

using namespace esp32_ble_client;

// Legacy versions:
// Version 1: Initial version without active connections
// Version 2: Support for active connections
// Version 3: New connection API
// Version 4: Pairing support
// Version 5: Cache clear support
static const uint32_t LEGACY_ACTIVE_CONNECTIONS_VERSION = 5;
static const uint32_t LEGACY_PASSIVE_ONLY_VERSION = 1;

enum BluetoothProxyFeature : uint32_t {
  FEATURE_PASSIVE_SCAN = 1 << 0,
  FEATURE_ACTIVE_CONNECTIONS = 1 << 1,
  FEATURE_REMOTE_CACHING = 1 << 2,
  FEATURE_PAIRING = 1 << 3,
  FEATURE_CACHE_CLEARING = 1 << 4,
  FEATURE_RAW_ADVERTISEMENTS = 1 << 5,
};

enum BluetoothProxySubscriptionFlag : uint32_t {
  SUBSCRIPTION_RAW_ADVERTISEMENTS = 1 << 0,
};

class BluetoothProxy : public esp32_ble_tracker::ESPBTDeviceListener, public Component {
 public:
  BluetoothProxy();
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  bool parse_devices(esp_ble_gap_cb_param_t::ble_scan_result_evt_param *advertisements, size_t count) override;
  void dump_config() override;
  void loop() override;
  esp32_ble_tracker::AdvertisementParserType get_advertisement_parser_type() override;

  void register_connection(BluetoothConnection *connection) {
    this->connections_.push_back(connection);
    connection->proxy_ = this;
  }

  void bluetooth_device_request(const api::BluetoothDeviceRequest &msg);
  void bluetooth_gatt_read(const api::BluetoothGATTReadRequest &msg);
  void bluetooth_gatt_write(const api::BluetoothGATTWriteRequest &msg);
  void bluetooth_gatt_read_descriptor(const api::BluetoothGATTReadDescriptorRequest &msg);
  void bluetooth_gatt_write_descriptor(const api::BluetoothGATTWriteDescriptorRequest &msg);
  void bluetooth_gatt_send_services(const api::BluetoothGATTGetServicesRequest &msg);
  void bluetooth_gatt_notify(const api::BluetoothGATTNotifyRequest &msg);

  int get_bluetooth_connections_free();
  int get_bluetooth_connections_limit() { return this->connections_.size(); }

  void subscribe_api_connection(api::APIConnection *api_connection, uint32_t flags);
  void unsubscribe_api_connection(api::APIConnection *api_connection);
  api::APIConnection *get_api_connection() { return this->api_connection_; }

  void send_device_connection(uint64_t address, bool connected, uint16_t mtu = 0, esp_err_t error = ESP_OK);
  void send_connections_free();
  void send_gatt_services_done(uint64_t address);
  void send_gatt_error(uint64_t address, uint16_t handle, esp_err_t error);
  void send_device_pairing(uint64_t address, bool paired, esp_err_t error = ESP_OK);
  void send_device_unpairing(uint64_t address, bool success, esp_err_t error = ESP_OK);
  void send_device_clear_cache(uint64_t address, bool success, esp_err_t error = ESP_OK);

  static void uint64_to_bd_addr(uint64_t address, esp_bd_addr_t bd_addr) {
    bd_addr[0] = (address >> 40) & 0xff;
    bd_addr[1] = (address >> 32) & 0xff;
    bd_addr[2] = (address >> 24) & 0xff;
    bd_addr[3] = (address >> 16) & 0xff;
    bd_addr[4] = (address >> 8) & 0xff;
    bd_addr[5] = (address >> 0) & 0xff;
  }

  void set_active(bool active) { this->active_ = active; }
  bool has_active() { return this->active_; }

  uint32_t get_legacy_version() const {
    if (this->active_) {
      return LEGACY_ACTIVE_CONNECTIONS_VERSION;
    }
    return LEGACY_PASSIVE_ONLY_VERSION;
  }

  uint32_t get_feature_flags() const {
    uint32_t flags = 0;
    flags |= BluetoothProxyFeature::FEATURE_PASSIVE_SCAN;
    flags |= BluetoothProxyFeature::FEATURE_RAW_ADVERTISEMENTS;
    if (this->active_) {
      flags |= BluetoothProxyFeature::FEATURE_ACTIVE_CONNECTIONS;
      flags |= BluetoothProxyFeature::FEATURE_REMOTE_CACHING;
      flags |= BluetoothProxyFeature::FEATURE_PAIRING;
      flags |= BluetoothProxyFeature::FEATURE_CACHE_CLEARING;
    }

    return flags;
  }

  std::string get_bluetooth_mac_address_pretty() {
    const uint8_t *mac = esp_bt_dev_get_address();
    return str_snprintf("%02X:%02X:%02X:%02X:%02X:%02X", 17, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }

 protected:
  void send_api_packet_(const esp32_ble_tracker::ESPBTDevice &device);

  BluetoothConnection *get_connection_(uint64_t address, bool reserve);

  bool active_;

  std::vector<BluetoothConnection *> connections_{};
  api::APIConnection *api_connection_{nullptr};
  bool raw_advertisements_{false};
};

extern BluetoothProxy *global_bluetooth_proxy;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace bluetooth_proxy
}  // namespace esphome

#endif  // USE_ESP32
