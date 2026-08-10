#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BLUETOOTH_PROXY

#include <array>

#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_pb2.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

#include "esphome/components/bluetooth_connection/bluetooth_connection.h"

#include "esphome/components/ble_device_base/ble_hub_impl.h"

#include "esphome/components/bluetooth_connection/bluetooth_connection_hub.h"

namespace esphome::bluetooth_proxy {

// The connection-domain types live in the bluetooth_connection component;
// re-exported here so the proxy code reads unqualified.
using bluetooth_connection::CONN_OK;
using bluetooth_connection::conn_err_t;
using bluetooth_connection::GATT_NOT_CONNECTED;
using bluetooth_connection::INIT_SENDING_SERVICES;

#ifdef BLUETOOTH_CONNECTION_HAS_GATT
using BluetoothConnection = bluetooth_connection::BluetoothConnection;
using ClientState = ble_device_base::ClientState;
#endif

// Legacy versions:
// Version 1: Initial version without active connections
// Version 2: Support for active connections
// Version 3: New connection API
// Version 4: Pairing support
// Version 5: Cache clear support
static constexpr uint32_t LEGACY_ACTIVE_CONNECTIONS_VERSION = 5;
static constexpr uint32_t LEGACY_ACTIVE_NO_CACHE_CLEAR_VERSION = 4;
static constexpr uint32_t LEGACY_ACTIVE_NO_PAIRING_VERSION = 3;
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

class BluetoothProxy final : public Component {
#ifdef BLUETOOTH_CONNECTION_HAS_GATT
  // Allow the connection to update connections_free_response_
  friend bluetooth_connection::BluetoothConnection;
#endif
 public:
  BluetoothProxy();
  void set_ble_hub(ble_device_base::BLEHub *hub) { this->hub_ = hub; }
  void dump_config() override;
  void setup() override;
  void loop() override;

#ifdef BLUETOOTH_CONNECTION_HAS_GATT
  void register_connection(BluetoothConnection *connection);
#endif  // BLUETOOTH_CONNECTION_HAS_GATT
#ifndef USE_ESP32
  // Run after the hub's setup() (the trackers use AFTER_WIFI): setup() below
  // snapshots scan_active()/scan_running() and installs the raw callback, and
  // the BLEHub contract does not promise those are settled any earlier than
  // the hub's own setup().
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI - 1.0f; }
#endif  // !USE_ESP32

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
  /// Whether the subscribed API client understands 16/32-bit UUID fields.
  bool client_supports_efficient_uuids() const {
    return this->api_connection_ != nullptr && this->api_connection_->client_supports_api_version(1, 12);
  }

  void send_device_connection(uint64_t address, bool connected, uint16_t mtu = 0, conn_err_t error = CONN_OK);
  void send_connections_free();
  void send_connections_free(api::APIConnection *api_connection);
  void send_gatt_services_done(uint64_t address);
  void send_gatt_error(uint64_t address, uint16_t handle, conn_err_t error);
  void send_device_pairing(uint64_t address, bool paired, conn_err_t error = CONN_OK);
  void send_device_unpairing(uint64_t address, bool success, conn_err_t error = CONN_OK);
  void send_device_clear_cache(uint64_t address, bool success, conn_err_t error = CONN_OK);

  void bluetooth_scanner_set_mode(bool active);

  void set_active(bool active) { this->active_ = active; }
  bool has_active() { return this->active_; }

  uint32_t get_legacy_version() const {
    if (!this->active_) {
      return LEGACY_PASSIVE_ONLY_VERSION;
    }
    // Legacy clients (which predate the feature flags) map versions to
    // capability sets: 5 adds cache clearing, 4 adds pairing, 3 is active
    // connections only.
    if (bluetooth_connection::SUPPORTS_CACHE_CLEARING) {
      return LEGACY_ACTIVE_CONNECTIONS_VERSION;
    }
    return bluetooth_connection::SUPPORTS_PAIRING ? LEGACY_ACTIVE_NO_CACHE_CLEAR_VERSION
                                                  : LEGACY_ACTIVE_NO_PAIRING_VERSION;
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
    if (ble_device_base::BLEHub::get_capabilities().scan_mode_switch) {
      flags |= BluetoothProxyFeature::FEATURE_STATE_AND_MODE;
    }
#endif
    if (this->active_) {
      // REMOTE_CACHING is mandatory for active connections: API clients
      // refuse to connect without it (it selects which V3 connect request
      // they send, not device-side caching).
      flags |= BluetoothProxyFeature::FEATURE_ACTIVE_CONNECTIONS;
      flags |= BluetoothProxyFeature::FEATURE_REMOTE_CACHING;
      flags |= BluetoothProxyFeature::FEATURE_CONNECTION_PARAMS_SETTING;
      if (bluetooth_connection::SUPPORTS_PAIRING) {
        flags |= BluetoothProxyFeature::FEATURE_PAIRING;
      }
      if (bluetooth_connection::SUPPORTS_CACHE_CLEARING) {
        flags |= BluetoothProxyFeature::FEATURE_CACHE_CLEARING;
      }
    }

    return flags;
  }

  void get_bluetooth_mac_address_pretty(std::span<char, 18> output) {
    uint8_t mac[6] = {};
    this->hub_->get_adapter_mac(mac);
    // Unavailable -> empty string: some hubs (rp2040's BTstack) only learn
    // the address once the link layer is up, and report all-zero until then.
    if (mac_address_is_valid(mac)) {
      format_mac_addr_upper(mac, output.data());
    } else {
      output[0] = '\0';
    }
  }

 protected:
  bool send_bluetooth_scanner_state_(ble_device_base::ScannerState state);
#ifndef USE_BLE_SCANNER_STATE_CALLBACK
  void send_polled_scanner_state_();
#endif
  void on_raw_advertisement_(const ble_device_base::RawAdvertisement &raw);

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

#ifdef BLUETOOTH_CONNECTION_HAS_GATT
  BluetoothConnection *get_connection_(uint64_t address, bool reserve);
  void log_connection_request_ignored_(BluetoothConnection *connection, ClientState state);
  void log_connection_info_(BluetoothConnection *connection, const char *message);
#endif
  void log_not_connected_gatt_(const char *action, const char *type);
  void handle_gatt_not_connected_(uint64_t address, uint16_t handle, const char *action, const char *type);

#ifdef BLUETOOTH_CONNECTION_HAS_GATT
  /// Keep the pre-allocated connections-free message in step when a
  /// connection slot changes address (0 = free). Called from the connection
  /// classes' set_address().
  // maybe_unused + guard: in a passive proxy (active: false) MAX is 0, the
  // body is removed, and the free < MAX compare would trip -Wtype-limits.
  void update_address_slot_([[maybe_unused]] uint64_t old_address, [[maybe_unused]] uint64_t new_address) {
#if BLUETOOTH_PROXY_MAX_CONNECTIONS > 0
    auto &resp = this->connections_free_response_;
    if (new_address == 0 && old_address != 0) {
      if (resp.free < BLUETOOTH_PROXY_MAX_CONNECTIONS) {
        resp.free++;
      } else {
        this->log_slot_accounting_mismatch_();
      }
      this->replace_allocated_slot_(old_address, 0);
    } else if (new_address != 0 && old_address == 0) {
      if (resp.free > 0) {
        resp.free--;
      } else {
        this->log_slot_accounting_mismatch_();
      }
      this->replace_allocated_slot_(0, new_address);
    }
#endif  // BLUETOOTH_PROXY_MAX_CONNECTIONS > 0
  }
  void replace_allocated_slot_(uint64_t find_value, uint64_t set_value);
  void log_slot_accounting_mismatch_();
  /// Free a connection slot after teardown: notify the API client and reset
  /// the streaming cursor. Important: does NOT send send_gatt_services_done()
  /// when service streaming was interrupted -- the client (aioesphomeapi) has
  /// a 30-second timeout (DEFAULT_BLE_TIMEOUT) to detect incomplete service
  /// discovery and retry, rather than being told a partial list is complete.
  void reset_connection_slot_(BluetoothConnection *connection, conn_err_t reason);
#endif

  // Memory optimized layout for 32-bit systems
  // Group 1: Pointers (4 bytes each, naturally aligned)
  api::APIConnection *api_connection_{nullptr};

#ifdef BLUETOOTH_CONNECTION_HAS_GATT
  // Group 2: Fixed-size array of connection pointers
  std::array<BluetoothConnection *, BLUETOOTH_PROXY_MAX_CONNECTIONS> connections_{};
#endif
  ble_device_base::BLEHub *hub_{nullptr};
  // Group 3: 4-byte types; paired with hub_ so the 8-aligned messages below
  // start on an even word, closing two alignment holes.
  uint32_t last_advertisement_flush_time_{0};

  // BLE advertisement batching
  api::BluetoothLERawAdvertisementsResponse response_;

  // Pre-allocated response message - always ready to send
  api::BluetoothConnectionsFreeResponse connections_free_response_;

  // Group 4: 1-byte types grouped together
  bool active_;
  // A dropped send (full TCP buffer) would leave the API client with a stale
  // slot state forever; the cached response is current by construction, so
  // retrying it from loop() is an idempotent resync.
  bool connections_free_pending_{false};
  uint8_t connection_count_{0};
  bool configured_scan_active_{false};  // Configured scan mode from YAML
#ifndef USE_BLE_SCANNER_STATE_CALLBACK
  bool last_scan_running_{false};  // Last scanner state reported to the subscriber
#endif
};

extern BluetoothProxy *global_bluetooth_proxy;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::bluetooth_proxy

#endif  // USE_BLUETOOTH_PROXY
