#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BLUETOOTH_PROXY

#include <array>
#include <map>
#include <vector>

#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_pb2.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

#include "esphome/components/ble_device_base/ble_client_state.h"

#ifdef USE_ESP32
#include "esphome/components/esp32_ble_client/ble_client_base.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#include "bluetooth_connection.h"

#ifndef CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID
#include <esp_bt.h>
#endif
#include <esp_bt_device.h>
#else
#include "esphome/components/ble_device_base/ble_hub.h"
#endif  // USE_ESP32

namespace esphome::bluetooth_proxy {

// Proxy-owned error type for the API error fields, which are plain integers on
// the wire. Aliases esp_err_t on esp32 (where the values come from IDF calls);
// a bare int elsewhere. Owning the name instead of probing for esp_err_t keeps
// the header independent of how a hub platform's SDK spells its error type.
#ifdef USE_ESP32
using proxy_err_t = esp_err_t;
static constexpr proxy_err_t PROXY_OK = ESP_OK;
#else
using proxy_err_t = int;
static constexpr proxy_err_t PROXY_OK = 0;
#endif

static constexpr proxy_err_t ESP_GATT_NOT_CONNECTED = ble_device_base::GATT_ERR_NOT_CONNECTED;
static constexpr int DONE_SENDING_SERVICES = -2;
static constexpr int INIT_SENDING_SERVICES = -3;

#ifdef USE_ESP32
using namespace esp32_ble_client;
#endif

// Legacy versions:
// Version 1: Initial version without active connections
// Version 2: Support for active connections
// Version 3: New connection API
// Version 4: Pairing support
// Version 5: Cache clear support
static constexpr uint32_t LEGACY_ACTIVE_CONNECTIONS_VERSION = 5;
static constexpr uint32_t LEGACY_PASSIVE_ONLY_VERSION = 1;

enum BluetoothProxyFeature : uint32_t {
  FEATURE_PASSIVE_SCAN = 1 << 0,
  FEATURE_ACTIVE_CONNECTIONS = 1 << 1,
  FEATURE_REMOTE_CACHING = 1 << 2,
  FEATURE_PAIRING = 1 << 3,
  FEATURE_CACHE_CLEARING = 1 << 4,
  FEATURE_RAW_ADVERTISEMENTS = 1 << 5,
  FEATURE_STATE_AND_MODE = 1 << 6,
  FEATURE_CONNECTION_PARAMS_SETTING = 1 << 7,
};

enum BluetoothProxySubscriptionFlag : uint32_t {
  SUBSCRIPTION_RAW_ADVERTISEMENTS = 1 << 0,
};

#ifdef USE_ESP32
class BluetoothProxy final : public esp32_ble_tracker::ESPBTDeviceListener,
                             public esp32_ble_tracker::BLEScannerStateListener,
                             public Component {
  friend class BluetoothConnection;  // Allow connection to update connections_free_response_
#else
class BluetoothProxy final : public Component {
#endif
 public:
  BluetoothProxy();
#ifdef USE_ESP32
#ifdef USE_ESP32_BLE_DEVICE
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
#endif
  bool parse_devices(const esp32_ble::BLEScanResult *scan_results, size_t count) override;
  esp32_ble_tracker::AdvertisementParserType get_advertisement_parser_type() override;
#endif  // USE_ESP32
  void dump_config() override;
  void setup() override;
  void loop() override;

#ifdef USE_ESP32
  // maybe_unused: in a passive proxy (active: false) MAX is 0, the body below is removed, and connection is unused.
  void register_connection([[maybe_unused]] BluetoothConnection *connection) {
    // Guard the always-false comparison (-Wtype-limits) in a passive proxy (active: false), where MAX is 0.
#if BLUETOOTH_PROXY_MAX_CONNECTIONS > 0
    if (this->connection_count_ < BLUETOOTH_PROXY_MAX_CONNECTIONS) {
      this->connections_[this->connection_count_++] = connection;
      connection->proxy_ = this;
    }
#endif
  }
#else
  void set_ble_hub(ble_device_base::BLEHub *hub) { this->hub_ = hub; }
  // Run after the hub's setup() (the trackers use AFTER_WIFI): setup() below
  // snapshots scan_active()/scan_running() and installs the raw callback, and
  // the BLEHub contract does not promise those are settled any earlier than
  // the hub's own setup().
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI - 1.0f; }
#endif  // USE_ESP32

  void bluetooth_device_request(const api::BluetoothDeviceRequest &msg);
  void bluetooth_gatt_read(const api::BluetoothGATTReadRequest &msg);
  void bluetooth_gatt_write(const api::BluetoothGATTWriteRequest &msg);
  void bluetooth_gatt_read_descriptor(const api::BluetoothGATTReadDescriptorRequest &msg);
  void bluetooth_gatt_write_descriptor(const api::BluetoothGATTWriteDescriptorRequest &msg);
  void bluetooth_gatt_send_services(const api::BluetoothGATTGetServicesRequest &msg);
  void bluetooth_gatt_notify(const api::BluetoothGATTNotifyRequest &msg);
  void bluetooth_set_connection_params(const api::BluetoothSetConnectionParamsRequest &msg);

  void subscribe_api_connection(api::APIConnection *api_connection, uint32_t flags);
  void unsubscribe_api_connection(api::APIConnection *api_connection);
  api::APIConnection *get_api_connection() { return this->api_connection_; }

  void send_device_connection(uint64_t address, bool connected, uint16_t mtu = 0, proxy_err_t error = PROXY_OK);
  void send_connections_free();
  void send_connections_free(api::APIConnection *api_connection);
  void send_gatt_services_done(uint64_t address);
  void send_gatt_error(uint64_t address, uint16_t handle, proxy_err_t error);
  void send_device_pairing(uint64_t address, bool paired, proxy_err_t error = PROXY_OK);
  void send_device_unpairing(uint64_t address, bool success, proxy_err_t error = PROXY_OK);
  void send_device_clear_cache(uint64_t address, bool success, proxy_err_t error = PROXY_OK);

  void bluetooth_scanner_set_mode(bool active);

#ifdef USE_ESP32
  static void uint64_to_bd_addr(uint64_t address, esp_bd_addr_t bd_addr) {
    bd_addr[0] = (address >> 40) & 0xff;
    bd_addr[1] = (address >> 32) & 0xff;
    bd_addr[2] = (address >> 24) & 0xff;
    bd_addr[3] = (address >> 16) & 0xff;
    bd_addr[4] = (address >> 8) & 0xff;
    bd_addr[5] = (address >> 0) & 0xff;
  }
#endif

  void set_active(bool active) { this->active_ = active; }
  bool has_active() { return this->active_; }

#ifdef USE_ESP32
  /// BLEScannerStateListener interface
  void on_scanner_state(esp32_ble_tracker::ScannerState state) override;
#endif

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
#ifdef USE_ESP32
    flags |= BluetoothProxyFeature::FEATURE_STATE_AND_MODE;
#else
    // Advertise mode switching only where the hub honors request_scan_mode();
    // scan_mode_switch is the capability bit for exactly that (#18079) —
    // active_scan alone is not enough, a hub may support active scanning yet
    // refuse the runtime switch.
    if (this->hub_->get_capabilities().scan_mode_switch) {
      flags |= BluetoothProxyFeature::FEATURE_STATE_AND_MODE;
    }
#endif
    if (this->active_) {
      flags |= BluetoothProxyFeature::FEATURE_ACTIVE_CONNECTIONS;
      flags |= BluetoothProxyFeature::FEATURE_REMOTE_CACHING;
      flags |= BluetoothProxyFeature::FEATURE_PAIRING;
      flags |= BluetoothProxyFeature::FEATURE_CACHE_CLEARING;
      flags |= BluetoothProxyFeature::FEATURE_CONNECTION_PARAMS_SETTING;
    }

    return flags;
  }

  void get_bluetooth_mac_address_pretty(std::span<char, 18> output) {
#ifdef USE_ESP32
    const uint8_t *mac = esp_bt_dev_get_address();
    if (mac != nullptr) {
      format_mac_addr_upper(mac, output.data());
    } else {
      output[0] = '\0';
    }
#else
    uint8_t mac[6] = {};
    this->hub_->get_adapter_mac(mac);
    // Mirror the esp32 arm's unavailable -> empty-string fallback: some hubs
    // (rp2040's BTstack) only learn the address once the link layer is up, and
    // report all-zero until then.
    bool nonzero = false;
    for (uint8_t b : mac)
      nonzero |= b != 0;
    if (nonzero) {
      format_mac_addr_upper(mac, output.data());
    } else {
      output[0] = '\0';
    }
#endif
  }

 protected:
#ifdef USE_ESP32
  void send_bluetooth_scanner_state_(esp32_ble_tracker::ScannerState state);
#else
  void send_bluetooth_scanner_state_();
  void on_raw_advertisement_(const ble_device_base::RawAdvertisement &raw);
#endif

  /// Caller must ensure api_connection_ is non-null and API server is connected.
  void flush_pending_advertisements_() {
    if (this->response_.advertisements_len == 0)
      return;
    this->api_connection_->send_message(this->response_);
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    this->log_advertisement_flush_();
#endif
    this->response_.advertisements_len = 0;
  }
  void log_advertisement_flush_();

#ifdef USE_ESP32
  BluetoothConnection *get_connection_(uint64_t address, bool reserve);
  void log_connection_request_ignored_(BluetoothConnection *connection, espbt::ClientState state);
  void log_connection_info_(BluetoothConnection *connection, const char *message);
#endif
  void log_not_connected_gatt_(const char *action, const char *type);
  void handle_gatt_not_connected_(uint64_t address, uint16_t handle, const char *action, const char *type);

  // Memory optimized layout for 32-bit systems
  // Group 1: Pointers (4 bytes each, naturally aligned)
  api::APIConnection *api_connection_{nullptr};

#ifdef USE_ESP32
  // Group 2: Fixed-size array of connection pointers
  std::array<BluetoothConnection *, BLUETOOTH_PROXY_MAX_CONNECTIONS> connections_{};
#else
  ble_device_base::BLEHub *hub_{nullptr};
#endif

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
#ifndef USE_ESP32
  bool last_scan_running_{false};  // Last scanner state reported to the subscriber
#endif
};

extern BluetoothProxy *global_bluetooth_proxy;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::bluetooth_proxy

#endif  // USE_BLUETOOTH_PROXY
