#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BLUETOOTH_PROXY

#include <array>

#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_pb2.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "esphome/components/bluetooth_connection/bluetooth_connection.h"

#include "esphome/components/ble_device_base/ble_hub_impl.h"

#include "esphome/components/bluetooth_connection/bluetooth_connection_hub.h"

namespace esphome::bluetooth_proxy {

// The connection-domain types live in the bluetooth_connection component;
// re-exported here so the proxy code reads unqualified.
using bluetooth_connection::CONN_OK;
using bluetooth_connection::conn_err_t;
using bluetooth_connection::GATT_NOT_CONNECTED;
using bluetooth_connection::DONE_SENDING_SERVICES;
using bluetooth_connection::INIT_SENDING_SERVICES;
using bluetooth_connection::SERVICES_DONE_PENDING;

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
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

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
/// One owed address-keyed reply in a single word: 48-bit address low, 16-bit
/// error on top. Every error that reaches it fits int16_t.
class PendingReply {
 public:
  constexpr void set(uint64_t address, conn_err_t error) {
    // Mask: the address originates from the client, and a stray high bit
    // must not corrupt the reason.
    this->word_ = (address & ADDRESS_MASK) | (static_cast<uint64_t>(static_cast<uint16_t>(error)) << 48);
  }
  constexpr void clear() { this->word_ = 0; }
  // Whole-word test: only (address 0, error 0) reads back as nothing owed.
  // A zero-address failure still latches, which is correct - that reply is
  // owed too. Neither backend can unpair address 0 successfully.
  constexpr bool empty() const { return this->word_ == 0; }
  // Masked like set(), so a stray high bit cannot defeat the pool lookups.
  constexpr bool matches(uint64_t address) const { return this->address() == (address & ADDRESS_MASK); }
  constexpr uint64_t address() const { return this->word_ & ADDRESS_MASK; }
  constexpr conn_err_t error() const { return static_cast<int16_t>(this->word_ >> 48); }

 private:
  static constexpr uint64_t ADDRESS_MASK = 0x0000FFFFFFFFFFFFULL;
  uint64_t word_{0};
};
// Pin the packing at compile time: mask and sign round-trip for every
// reachable shape (negative, GATT status, ESP_ERR_* range, stray high bit).
constexpr bool pending_reply_round_trips(uint64_t address, uint64_t expected_address, conn_err_t error) {
  PendingReply p;
  p.set(address, error);
  return p.address() == expected_address && p.error() == error && !p.empty() && p.matches(address);
}
static_assert(pending_reply_round_trips(0x0000112233445566ULL, 0x0000112233445566ULL, -1));
static_assert(pending_reply_round_trips(0x0000FFFFFFFFFFFFULL, 0x0000FFFFFFFFFFFFULL, 0x8F));
static_assert(pending_reply_round_trips(0xABCD112233445566ULL, 0x0000112233445566ULL, 0x110));
static_assert(PendingReply{}.empty());
#endif

class BluetoothProxy final : public Component {
#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
  // Allow the connection to update connections_free_response_
  friend bluetooth_connection::BluetoothConnection;
#endif
 public:
  BluetoothProxy();
  void set_ble_hub(ble_device_base::BLEHub *hub) { this->hub_ = hub; }
  void dump_config() override;
  void setup() override;
  void loop() override;

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
  void register_connection(BluetoothConnection *connection);
#endif  // USE_BLUETOOTH_PROXY_CONNECTIONS
#ifndef USE_ESP32
  // Run after the hub's setup() (the trackers use AFTER_WIFI): setup() below
  // snapshots scan_active()/scan_running() and installs the raw callback, and
  // the BLEHub contract does not promise those are settled any earlier than
  // the hub's own setup().
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI - 1.0f; }
#endif  // !USE_ESP32

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
  void bluetooth_device_request(const api::BluetoothDeviceRequest &msg);
  void bluetooth_gatt_read(const api::BluetoothGATTReadRequest &msg);
  void bluetooth_gatt_write(const api::BluetoothGATTWriteRequest &msg);
  void bluetooth_gatt_read_descriptor(const api::BluetoothGATTReadDescriptorRequest &msg);
  void bluetooth_gatt_write_descriptor(const api::BluetoothGATTWriteDescriptorRequest &msg);
  void bluetooth_gatt_send_services(const api::BluetoothGATTGetServicesRequest &msg);
  void bluetooth_gatt_notify(const api::BluetoothGATTNotifyRequest &msg);
  void bluetooth_set_connection_params(const api::BluetoothSetConnectionParamsRequest &msg);
#endif

  void subscribe_api_connection(api::APIConnection *api_connection, uint32_t flags);
  void unsubscribe_api_connection(api::APIConnection *api_connection);
  api::APIConnection *get_api_connection() { return this->api_connection_; }
  /// Whether the subscribed API client understands 16/32-bit UUID fields.
  bool client_supports_efficient_uuids() const {
    return this->api_connection_ != nullptr && this->api_connection_->client_supports_api_version(1, 12);
  }

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
  /// False only when a subscriber refused the frame; true = delivered or
  /// nobody subscribed. Refusals latch in send_device_disconnected_() and
  /// send_connected_reply_(); other callers report via log_reply_dropped_().
  bool send_device_connection(uint64_t address, bool connected, uint16_t mtu = 0, conn_err_t error = CONN_OK);
  void send_connections_free();
  void send_connections_free(api::APIConnection *api_connection);
  /// Same convention as send_device_connection: false only on a refused frame.
  bool send_gatt_services_done(uint64_t address);
  /// False only when the API refused the frame, so the reply is still owed.
  bool send_gatt_error(uint64_t address, uint16_t handle, conn_err_t error);
  void send_device_pairing(uint64_t address, bool paired, conn_err_t error = CONN_OK);
  /// No default error: the drain rebuilds success as (error == CONN_OK), so a
  /// caller that omitted it would have a reported failure resent as a success.
  void send_device_unpairing(uint64_t address, bool success, conn_err_t error);
  void send_device_clear_cache(uint64_t address, bool success, conn_err_t error = CONN_OK);
#endif

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

  void get_bluetooth_mac_address_pretty(std::span<char, MAC_ADDRESS_PRETTY_BUFFER_SIZE> output) {
    uint8_t mac[MAC_ADDRESS_SIZE] = {};
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
#ifdef USE_BLE_SCANNER_STATE_CALLBACK
  void send_scanner_state_(ble_device_base::ScannerState state);
#else
  void send_polled_scanner_state_();
#endif
  void on_raw_advertisement_(const ble_device_base::RawAdvertisement &raw);

  /// Caller must ensure api_connection_ is non-null and API server is connected.
  void flush_pending_advertisements_() {
    if (this->response_.advertisements_len == 0)
      return;
    // Perishable and the highest-frequency send here: a drop only reports at
    // V, anything louder would be the flood the batch pacing exists to avoid.
    [[maybe_unused]] bool sent = this->api_connection_->send_message(this->response_);
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    this->log_advertisement_flush_(sent);
#endif
    this->response_.advertisements_len = 0;
  }
  void log_advertisement_flush_(bool sent);

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
  BluetoothConnection *get_connection_(uint64_t address, bool reserve);
  void log_connection_request_ignored_(BluetoothConnection *connection, ClientState state);
  void log_connection_info_(BluetoothConnection *connection, const char *message);
  void log_not_connected_gatt_(const char *action, const char *type);
  void handle_gatt_not_connected_(uint64_t address, uint16_t handle, const char *action, const char *type);

  /// Keep the pre-allocated connections-free message in step when a
  /// connection slot changes address (0 = free). Called from the connection
  /// classes' set_address().
  void update_address_slot_(uint64_t old_address, uint64_t new_address) {
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
  }
  void replace_allocated_slot_(uint64_t find_value, uint64_t set_value);
  void log_slot_accounting_mismatch_();
  /// Free a connection slot after teardown: notify the API client and reset
  /// the streaming cursor. Important: does NOT send send_gatt_services_done()
  /// when service streaming was interrupted -- the client (aioesphomeapi) has
  /// a 30-second timeout (DEFAULT_BLE_TIMEOUT) to detect incomplete service
  /// discovery and retry, rather than being told a partial list is complete.
  void reset_connection_slot_(BluetoothConnection *connection, conn_err_t reason);
  /// Drop any owed freed-slot notification for this address (client reconnected).
  void clear_pending_disconnection_(uint64_t address);
  /// Send connected=false and pool it for the paced drain if refused. A
  /// dropped disconnect desynchronises the proxy: the client keeps a link it
  /// believes is live and every operation on it times out. Unsolicited and
  /// drained notifications only; request answers use the variant below.
  void send_device_disconnected_(uint64_t address, conn_err_t error = CONN_OK);
  /// Answer a request with connected=false. Never pools: a refusal falls back
  /// to the client's request timeout, keeping the pool for the unsolicited
  /// notifications the client cannot recover on its own.
  void answer_device_disconnected_(uint64_t address);
  /// Pool a refused freed-slot notification for the paced drain.
  void latch_pending_disconnection_(uint64_t address, conn_err_t error);
#endif

  /// Drop everything the ending session was owed. One list, so a new latch is
  /// one edit rather than two call sites where an omission looks deliberate.
  /// Drops state only, never sends: api_connection_ is the departing
  /// subscriber on subscribe and nullptr on unsubscribe.
  void reset_owed_replies_();
#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
  /// Report a reply we deliberately do not latch, so no drop is silent.
  void log_reply_dropped_(const char *what, uint64_t address);
  /// A latched reply's leading edge; the drain's re-refusals stay quiet.
  void log_reply_deferred_(const char *what, uint64_t address);
  /// A latched reply lost to a newer one for a different address.
  void log_reply_displaced_(const char *what, uint64_t owed, uint64_t address);
#endif

  // Memory optimized layout for 32-bit systems
  // Group 1: Pointers (4 bytes each, naturally aligned)
  api::APIConnection *api_connection_{nullptr};

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
  // Group 2: Fixed-size array of connection pointers
  std::array<BluetoothConnection *, BLUETOOTH_PROXY_MAX_CONNECTIONS> connections_{};
  // Address-keyed pool of owed freed-slot notifications; loop() resends.
  // Proxy-only state, kept off BluetoothConnection; entries are not tied to
  // slot indices.
  std::array<PendingReply, BLUETOOTH_PROXY_MAX_CONNECTIONS> pending_disconnections_{};
  // Owed unpair reply. The bond is already gone when the send is refused, so
  // a retry is told the unpair failed when it succeeded. One slot: a second
  // refused unpair displaces the first, as happened to both before this.
  PendingReply pending_unpairing_{};
#endif
  ble_device_base::BLEHub *hub_{nullptr};
  // Group 3: 4-byte types; paired with hub_ so the 8-aligned messages below
  // start on an even word, closing two alignment holes.
  uint32_t last_advertisement_flush_time_{0};

  // BLE advertisement batching
  api::BluetoothLERawAdvertisementsResponse response_;

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
  // Pre-allocated response message - always ready to send
  api::BluetoothConnectionsFreeResponse connections_free_response_;
#endif

  // Group 4: 1-byte types grouped together
  bool active_;
#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
  // A dropped send (full TCP buffer) would leave the API client with a stale
  // slot state forever; the cached response is current by construction, so
  // retrying it from loop() is an idempotent resync.
  bool connections_free_pending_{false};
  uint8_t connection_count_{0};
#endif
  bool configured_scan_active_{false};  // Configured scan mode from YAML
#ifdef USE_WIFI
  /// Wi-Fi only: flush on every other non-empty tick (~200 ms) so partial
  /// batches fill; an idle tick re-arms, so the first batch after a gap
  /// still ships on the next tick. See loop().
  bool adv_flush_toggle_{false};
#endif
#ifdef USE_BLE_SCANNER_STATE_CALLBACK
  // A dropped push (full TX buffer) is re-queried from the hub and resent
  // from loop(); the hub's current state is idempotent by construction.
  bool scanner_state_pending_{false};
#else
  bool last_scan_running_{false};  // Last scanner state reported to the subscriber
#endif
};

extern BluetoothProxy *global_bluetooth_proxy;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::bluetooth_proxy

#endif  // USE_BLUETOOTH_PROXY
