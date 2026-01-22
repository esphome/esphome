#pragma once

#ifdef USE_ESP32

#include <array>
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

#ifndef CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID
#include <esp_bt.h>
#endif
#include <esp_bt_device.h>

namespace esphome::bluetooth_proxy {

static const esp_err_t ESP_GATT_NOT_CONNECTED = -1;
static const int DONE_SENDING_SERVICES = -2;
static const int INIT_SENDING_SERVICES = -3;

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
  FEATURE_STATE_AND_MODE = 1 << 6,
};

enum BluetoothProxySubscriptionFlag : uint32_t {
  SUBSCRIPTION_RAW_ADVERTISEMENTS = 1 << 0,
};

class BluetoothProxy final : public esp32_ble_tracker::ESPBTDeviceListener,
                             public esp32_ble_tracker::BLEScannerStateListener,
                             public Component {
  friend class BluetoothConnection;  // Allow connection to update connections_free_response_
 public:
  BluetoothProxy();
#ifdef USE_ESP32_BLE_DEVICE
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
#endif
  bool parse_devices(const esp32_ble::BLEScanResult *scan_results, size_t count) override;
  void dump_config() override;
  void setup() override;
  void loop() override;
  void flush_pending_advertisements();
  esp32_ble_tracker::AdvertisementParserType get_advertisement_parser_type() override;

  void register_connection(BluetoothConnection *connection) {
    if (this->connection_count_ < BLUETOOTH_PROXY_MAX_CONNECTIONS) {
      this->connections_[this->connection_count_++] = connection;
      connection->proxy_ = this;
    }
  }

  void bluetooth_device_request(const api::BluetoothDeviceRequest &msg);
  void bluetooth_gatt_read(const api::BluetoothGATTReadRequest &msg);
  void bluetooth_gatt_write(const api::BluetoothGATTWriteRequest &msg);
  void bluetooth_gatt_read_descriptor(const api::BluetoothGATTReadDescriptorRequest &msg);
  void bluetooth_gatt_write_descriptor(const api::BluetoothGATTWriteDescriptorRequest &msg);
  void bluetooth_gatt_send_services(const api::BluetoothGATTGetServicesRequest &msg);
  void bluetooth_gatt_notify(const api::BluetoothGATTNotifyRequest &msg);

  void subscribe_api_connection(api::APIConnection *api_connection, uint32_t flags);
  void unsubscribe_api_connection(api::APIConnection *api_connection);
  api::APIConnection *get_api_connection() { return this->api_connection_; }

  void send_device_connection(uint64_t address, bool connected, uint16_t mtu = 0, esp_err_t error = ESP_OK);
  void send_connections_free();
  void send_connections_free(api::APIConnection *api_connection);
  void send_gatt_services_done(uint64_t address);
  void send_gatt_error(uint64_t address, uint16_t handle, esp_err_t error);
  void send_device_pairing(uint64_t address, bool paired, esp_err_t error = ESP_OK);
  void send_device_unpairing(uint64_t address, bool success, esp_err_t error = ESP_OK);
  void send_device_clear_cache(uint64_t address, bool success, esp_err_t error = ESP_OK);

  void bluetooth_scanner_set_mode(bool active);

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

  /// BLEScannerStateListener interface
  void on_scanner_state(esp32_ble_tracker::ScannerState state) override;

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
    flags |= BluetoothProxyFeature::FEATURE_STATE_AND_MODE;
    if (this->active_) {
      flags |= BluetoothProxyFeature::FEATURE_ACTIVE_CONNECTIONS;
      flags |= BluetoothProxyFeature::FEATURE_REMOTE_CACHING;
      flags |= BluetoothProxyFeature::FEATURE_PAIRING;
      flags |= BluetoothProxyFeature::FEATURE_CACHE_CLEARING;
    }

    return flags;
  }

  void get_bluetooth_mac_address_pretty(std::span<char, 18> output) {
    const uint8_t *mac = esp_bt_dev_get_address();
    if (mac != nullptr) {
      format_mac_addr_upper(mac, output.data());
    } else {
      output[0] = '\0';
    }
  }

 protected:
  void send_bluetooth_scanner_state_(esp32_ble_tracker::ScannerState state);

  BluetoothConnection *get_connection_(uint64_t address, bool reserve);
  void log_connection_request_ignored_(BluetoothConnection *connection, espbt::ClientState state);
  void log_connection_info_(BluetoothConnection *connection, const char *message);
  void log_not_connected_gatt_(const char *action, const char *type);
  void handle_gatt_not_connected_(uint64_t address, uint16_t handle, const char *action, const char *type);

  // Memory optimized layout for 32-bit systems
  // Group 1: Pointers (4 bytes each, naturally aligned)
  api::APIConnection *api_connection_{nullptr};

  // Group 2: Fixed-size array of connection pointers
  std::array<BluetoothConnection *, BLUETOOTH_PROXY_MAX_CONNECTIONS> connections_{};

  // BLE advertisement batching
  api::BluetoothLERawAdvertisementsResponse response_;

  // Group 3: 4-byte types
  uint32_t last_advertisement_flush_time_{0};

  // Pre-allocated response message - always ready to send
  api::BluetoothConnectionsFreeResponse connections_free_response_;

  // Group 4: 1-byte types grouped together
  bool active_;
  uint8_t connection_count_{0};
  bool configured_scan_active_{false};  // Configured scan mode from YAML
  // 3 bytes used, 1 byte padding
};

extern BluetoothProxy *global_bluetooth_proxy;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::bluetooth_proxy

#endif  // USE_ESP32
