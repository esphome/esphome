#include "bluetooth_proxy.h"

#ifdef USE_BLUETOOTH_PROXY

#include "esphome/components/api/api_server.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/macros.h"
#include "esphome/core/application.h"
#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <limits>

namespace esphome::bluetooth_proxy {

static const char *const TAG = "bluetooth_proxy";

// BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE is defined during code generation
// It sets the batch size for BLE advertisements to maximize WiFi efficiency

// Verify BLE advertisement data array size matches the BLE specification (31 bytes adv + 31 bytes scan response)
static_assert(sizeof(((api::BluetoothLERawAdvertisement *) nullptr)->data) == 62,
              "BLE advertisement data array size mismatch");

BluetoothProxy::BluetoothProxy() { global_bluetooth_proxy = this; }

// The neutral enum's values are the wire values.
static_assert(static_cast<uint32_t>(ble_device_base::ScannerState::IDLE) == api::enums::BLUETOOTH_SCANNER_STATE_IDLE);
static_assert(static_cast<uint32_t>(ble_device_base::ScannerState::STARTING) ==
              api::enums::BLUETOOTH_SCANNER_STATE_STARTING);
static_assert(static_cast<uint32_t>(ble_device_base::ScannerState::RUNNING) ==
              api::enums::BLUETOOTH_SCANNER_STATE_RUNNING);
static_assert(static_cast<uint32_t>(ble_device_base::ScannerState::FAILED) ==
              api::enums::BLUETOOTH_SCANNER_STATE_FAILED);
static_assert(static_cast<uint32_t>(ble_device_base::ScannerState::STOPPING) ==
              api::enums::BLUETOOTH_SCANNER_STATE_STOPPING);
static_assert(static_cast<uint32_t>(ble_device_base::ScannerState::STOPPED) ==
              api::enums::BLUETOOTH_SCANNER_STATE_STOPPED);

bool BluetoothProxy::send_bluetooth_scanner_state_(ble_device_base::ScannerState state) {
  if (this->api_connection_ == nullptr)
    return false;
  api::BluetoothScannerStateResponse resp;
  resp.state = static_cast<api::enums::BluetoothScannerState>(state);
  resp.mode = this->hub_->scan_active() ? api::enums::BluetoothScannerMode::BLUETOOTH_SCANNER_MODE_ACTIVE
                                        : api::enums::BluetoothScannerMode::BLUETOOTH_SCANNER_MODE_PASSIVE;
  resp.configured_mode = this->configured_scan_active_
                             ? api::enums::BluetoothScannerMode::BLUETOOTH_SCANNER_MODE_ACTIVE
                             : api::enums::BluetoothScannerMode::BLUETOOTH_SCANNER_MODE_PASSIVE;
  return this->api_connection_->send_message(resp);
}

#ifndef USE_BLE_SCANNER_STATE_CALLBACK
void BluetoothProxy::send_polled_scanner_state_() {
  // One read feeds both the frame and the change detector; the detector only
  // advances if the frame was accepted, so a dropped send (WOULD_BLOCK on a
  // full TX buffer) is retried from loop() instead of leaving a stale state.
  const bool running = this->hub_->scan_running();
  if (this->send_bluetooth_scanner_state_(running ? ble_device_base::ScannerState::RUNNING
                                                  : ble_device_base::ScannerState::IDLE)) {
    this->last_scan_running_ = running;
  }
}
#endif  // !USE_BLE_SCANNER_STATE_CALLBACK

void BluetoothProxy::setup() {
  // BLUETOOTH_PROXY_MAX_CONNECTIONS is 0 on an advertisement-only proxy.
  this->connections_free_response_.limit = BLUETOOTH_PROXY_MAX_CONNECTIONS;
  this->connections_free_response_.free = BLUETOOTH_PROXY_MAX_CONNECTIONS;

  // Capture the configured scan mode from YAML before any API changes
  this->configured_scan_active_ = this->hub_->scan_active();

  this->hub_->set_raw_advertisement_callback({this, [](void *self, const ble_device_base::RawAdvertisement &adv) {
                                                static_cast<BluetoothProxy *>(self)->on_raw_advertisement_(adv);
                                              }});
#ifdef USE_BLE_SCANNER_STATE_CALLBACK
  // Only push hubs compile the slot; elsewhere loop() polls scan_running().
  this->hub_->set_scanner_state_callback({this, [](void *self, ble_device_base::ScannerState state) {
                                            static_cast<BluetoothProxy *>(self)->send_bluetooth_scanner_state_(state);
                                          }});
#endif
}

// The hub delivers raw advertisements on the ESPHome main loop.
void BluetoothProxy::on_raw_advertisement_(const ble_device_base::RawAdvertisement &raw) {
  if (!api::global_api_server->is_connected() || this->api_connection_ == nullptr)
    return;

  auto &adv = this->response_.advertisements[this->response_.advertisements_len];
  adv.address = raw.address;
  adv.rssi = raw.rssi;
  adv.address_type = raw.addr_type;
  uint8_t length = raw.data_len > sizeof(adv.data) ? sizeof(adv.data) : static_cast<uint8_t>(raw.data_len);
  adv.data_len = length;
  std::memcpy(adv.data, raw.data, length);

  this->response_.advertisements_len++;

  ESP_LOGV(TAG, "Queuing raw packet from %012" PRIX64 ", length %d. RSSI: %d dB", raw.address, length, raw.rssi);

  // Flush if we have reached BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE
  if (this->response_.advertisements_len >= BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE) {
    this->flush_pending_advertisements_();
  }
}

#ifdef BLUETOOTH_CONNECTION_HAS_GATT
void BluetoothProxy::log_connection_request_ignored_(BluetoothConnection *connection, ClientState state) {
  ESP_LOGW(TAG, "[%d] [%s] Connection request ignored, state: %s", connection->get_connection_index(),
           connection->address_str(), ble_device_base::client_state_to_string(state));
}

void BluetoothProxy::log_connection_info_(BluetoothConnection *connection, const char *message) {
  ESP_LOGI(TAG, "[%d] [%s] Connecting %s", connection->get_connection_index(), connection->address_str(), message);
}
#endif  // BLUETOOTH_CONNECTION_HAS_GATT

void BluetoothProxy::log_not_connected_gatt_(const char *action, const char *type) {
  ESP_LOGW(TAG, "Cannot %s GATT %s, not connected", action, type);
}

void BluetoothProxy::handle_gatt_not_connected_(uint64_t address, uint16_t handle, const char *action,
                                                const char *type) {
  this->log_not_connected_gatt_(action, type);
  this->send_gatt_error(address, handle, GATT_NOT_CONNECTED);
}

void BluetoothProxy::log_advertisement_flush_() {
  ESP_LOGV(TAG, "Sent batch of %u BLE advertisements", this->response_.advertisements_len);
}

void BluetoothProxy::dump_config() {
  // Print configured facts. dump_config runs right after setup, before the
  // radio is up, so live scan state would always read "stopped" here — the
  // loop's BluetoothScannerStateResponse carries the changing value instead.
  char mac_str[18];
  this->get_bluetooth_mac_address_pretty(mac_str);
  const char *mac_out = mac_str[0] != '\0' ? mac_str : "unavailable (adapter not up yet)";
  const char *scan_mode = this->configured_scan_active_ ? "active" : "passive";
#ifdef BLUETOOTH_CONNECTION_HAS_GATT
  ESP_LOGCONFIG(TAG,
                "Bluetooth Proxy:\n"
                "  Active: %s\n"
                "  Connections: %d\n"
                "  Configured scan: %s\n"
                "  Adapter MAC: %s",
                YESNO(this->active_), this->connection_count_, scan_mode, mac_out);
#else
  ESP_LOGCONFIG(TAG,
                "Bluetooth Proxy:\n"
                "  Mode: advertisement-only (no GATT connections)\n"
                "  Configured scan: %s\n"
                "  Adapter MAC: %s",
                scan_mode, mac_out);
#endif
}

#ifdef BLUETOOTH_CONNECTION_HAS_GATT

// maybe_unused: in a passive proxy (active: false) MAX is 0, the body is removed, and connection is unused.
void BluetoothProxy::register_connection([[maybe_unused]] BluetoothConnection *connection) {
// Guard the always-false comparison (-Wtype-limits) in a passive proxy (active: false), where MAX is 0.
#if BLUETOOTH_PROXY_MAX_CONNECTIONS > 0
  if (this->connection_count_ >= BLUETOOTH_PROXY_MAX_CONNECTIONS) {
    // Cannot happen with codegen-sized registration; a silent drop would
    // surface later as a null proxy_ dereference, so refuse loudly.
    ESP_LOGE(TAG, "Connection registry full, dropping registration");
    return;
  }
  // The hub wrapper has no Component lifecycle, so the index is assigned here.
  connection->connection_index_ = this->connection_count_;
  this->connections_[this->connection_count_++] = connection;
  connection->proxy_ = this;
#endif
}

void BluetoothProxy::log_slot_accounting_mismatch_() { ESP_LOGW(TAG, "Connection slot free-count mismatch, clamped"); }

void BluetoothProxy::replace_allocated_slot_(uint64_t find_value, uint64_t set_value) {
  for (auto &slot : this->connections_free_response_.allocated) {
    if (slot == find_value) {
      slot = set_value;
      return;
    }
  }
  // The accounting arrays are only mutated here and sized to the slot count,
  // so a miss means the bookkeeping already drifted — say so.
  ESP_LOGW(TAG, "Connection slot accounting mismatch (find 0x%llx)", (unsigned long long) find_value);
}

void BluetoothProxy::reset_connection_slot_(BluetoothConnection *connection, conn_err_t reason) {
  this->send_device_connection(connection->get_address(), false, 0, reason);
  connection->set_address(0);
  connection->send_service_ = INIT_SENDING_SERVICES;
  this->send_connections_free();
}

BluetoothConnection *BluetoothProxy::get_connection_(uint64_t address, bool reserve) {
  // Finish the scan before reserving: a free slot earlier in the array must
  // not win over a later slot that already holds the address, or one device
  // ends up on two slots with a second connection attempt racing the first.
  BluetoothConnection *free_slot = nullptr;
  for (uint8_t i = 0; i < this->connection_count_; i++) {
    auto *connection = this->connections_[i];
    uint64_t conn_addr = connection->get_address();

    if (conn_addr == address)
      return connection;

    if (free_slot == nullptr && conn_addr == 0)
      free_slot = connection;
  }
  if (!reserve || free_slot == nullptr)
    return nullptr;
  free_slot->send_service_ = INIT_SENDING_SERVICES;
  free_slot->set_address(address);
  // All connections must start at INIT
  // We only set the state if we allocate the connection
  // to avoid a race where multiple connection attempts
  // are made.
  free_slot->set_state(ClientState::INIT);
  return free_slot;
}

void BluetoothProxy::bluetooth_device_request(const api::BluetoothDeviceRequest &msg) {
  switch (msg.request_type) {
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITH_CACHE:
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITHOUT_CACHE: {
      auto *connection = this->get_connection_(msg.address, true);
      if (connection == nullptr) {
        ESP_LOGW(TAG, "No free connections available");
        this->send_device_connection(msg.address, false);
        return;
      }
      if (!msg.has_address_type) {
        ESP_LOGE(TAG, "[%d] [%s] Missing address type in connect request", connection->get_connection_index(),
                 connection->address_str());
        this->send_device_connection(msg.address, false);
        return;
      }
      if (connection->state() == ClientState::CONNECTED || connection->state() == ClientState::ESTABLISHED) {
        this->log_connection_request_ignored_(connection, connection->state());
        this->send_device_connection(msg.address, true);
        this->send_connections_free();
        return;
      } else if (connection->state() == ClientState::DISCONNECTING && connection->cancel_teardown()) {
        ESP_LOGW(TAG, "[%d] [%s] Connection request while pending disconnect, cancelling pending disconnect",
                 connection->get_connection_index(), connection->address_str());
        return;
      } else if (connection->state() != ClientState::INIT) {
        // Covers CONNECTING too: a repeat request during a connect attempt is
        // ignored the same way.
        this->log_connection_request_ignored_(connection, connection->state());
        return;
      }
      if (msg.request_type == api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITH_CACHE) {
        connection->set_connection_type(ble_device_base::ConnectionType::V3_WITH_CACHE);
        this->log_connection_info_(connection, "v3 with cache");
      } else {  // BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITHOUT_CACHE
        connection->set_connection_type(ble_device_base::ConnectionType::V3_WITHOUT_CACHE);
        this->log_connection_info_(connection, "v3 without cache");
      }
      connection->initiate_connection(static_cast<uint8_t>(msg.address_type));
      this->send_connections_free();
      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_DISCONNECT: {
      auto *connection = this->get_connection_(msg.address, false);
      if (connection == nullptr) {
        this->send_device_connection(msg.address, false);
        this->send_connections_free();
        return;
      }
      if (connection->state() != ClientState::IDLE) {
        connection->disconnect();
      } else {
        connection->set_address(0);
        this->send_device_connection(msg.address, false);
        this->send_connections_free();
      }
      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_PAIR: {
      // The connection wrapper exposes the pairing surface; success is
      // reported when the platform's pairing completion arrives.
      auto *connection = this->get_connection_(msg.address, false);
      if (connection != nullptr) {
        if (!connection->is_paired()) {
          auto err = connection->pair();
          if (err != CONN_OK) {
            this->send_device_pairing(msg.address, false, err);
          }
        } else {
          this->send_device_pairing(msg.address, true);
        }
      } else {
        // Answer instead of leaving the client to time out.
        this->send_device_pairing(msg.address, false, GATT_NOT_CONNECTED);
      }
      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_UNPAIR: {
      conn_err_t ret = bluetooth_connection::unpair_device(msg.address);
      if (ret == CONN_OK) {
        // The bond is gone; a live connection must not short-circuit the
        // next PAIR as already paired.
        auto *connection = this->get_connection_(msg.address, false);
        if (connection != nullptr) {
          connection->set_unpaired();
        }
      }
      this->send_device_unpairing(msg.address, ret == CONN_OK, ret);
      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CLEAR_CACHE: {
      conn_err_t ret = bluetooth_connection::clear_gatt_cache(msg.address);
      this->send_device_clear_cache(msg.address, ret == CONN_OK, ret);
      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT: {
      ESP_LOGE(TAG, "V1 connections removed");
      this->send_device_connection(msg.address, false);
      break;
    }
  }
}

void BluetoothProxy::bluetooth_gatt_read(const api::BluetoothGATTReadRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr) {
    this->handle_gatt_not_connected_(msg.address, msg.handle, "read", "characteristic");
    return;
  }

  auto err = connection->read_characteristic(msg.handle);
  if (err != CONN_OK) {
    this->send_gatt_error(msg.address, msg.handle, err);
  }
}

void BluetoothProxy::bluetooth_gatt_write(const api::BluetoothGATTWriteRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr) {
    this->handle_gatt_not_connected_(msg.address, msg.handle, "write", "characteristic");
    return;
  }

  auto err = connection->write_characteristic(msg.handle, msg.data, msg.data_len, msg.response);
  if (err != CONN_OK) {
    this->send_gatt_error(msg.address, msg.handle, err);
  }
}

void BluetoothProxy::bluetooth_gatt_read_descriptor(const api::BluetoothGATTReadDescriptorRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr) {
    this->handle_gatt_not_connected_(msg.address, msg.handle, "read", "descriptor");
    return;
  }

  auto err = connection->read_descriptor(msg.handle);
  if (err != CONN_OK) {
    this->send_gatt_error(msg.address, msg.handle, err);
  }
}

void BluetoothProxy::bluetooth_gatt_write_descriptor(const api::BluetoothGATTWriteDescriptorRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr) {
    this->handle_gatt_not_connected_(msg.address, msg.handle, "write", "descriptor");
    return;
  }

  auto err = connection->write_descriptor(msg.handle, msg.data, msg.data_len, true);
  if (err != CONN_OK) {
    this->send_gatt_error(msg.address, msg.handle, err);
  }
}

void BluetoothProxy::bluetooth_gatt_send_services(const api::BluetoothGATTGetServicesRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr || !connection->connected()) {
    this->handle_gatt_not_connected_(msg.address, 0, "get", "services");
    return;
  }
  if (!connection->has_gatt_services()) {
    ESP_LOGW(TAG, "[%d] [%s] No GATT services found", connection->get_connection_index(), connection->address_str());
    this->send_gatt_services_done(msg.address);
    return;
  }
  if (connection->send_service_ == INIT_SENDING_SERVICES)  // Start sending services if not started yet
    connection->send_service_ = 0;
}

void BluetoothProxy::bluetooth_gatt_notify(const api::BluetoothGATTNotifyRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr) {
    this->handle_gatt_not_connected_(msg.address, msg.handle, "notify", "characteristic");
    return;
  }

  auto err = connection->notify_characteristic(msg.handle, msg.enable);
  if (err != CONN_OK) {
    this->send_gatt_error(msg.address, msg.handle, err);
  }
}

void BluetoothProxy::bluetooth_set_connection_params(const api::BluetoothSetConnectionParamsRequest &msg) {
  if (this->api_connection_ == nullptr)
    return;
  // Send results unchecked (esp32 parity): a drop resolves via the client timeout.

  auto *connection = this->get_connection_(msg.address, false);
  api::BluetoothSetConnectionParamsResponse resp;
  resp.address = msg.address;

  if (connection == nullptr || !connection->connected()) {
    ESP_LOGW(TAG, "[%d] [%s] Cannot set connection params, not connected",
             connection ? static_cast<int>(connection->get_connection_index()) : -1,
             connection ? connection->address_str() : "unknown");
    resp.error = GATT_NOT_CONNECTED;
    this->api_connection_->send_message(resp);
    return;
  }

  // Protobuf fields are uint32_t to future-proof the API if BLE ever supports wider values;
  // clamp to uint16_t since the current BLE spec defines these as 16-bit.
  constexpr uint32_t max_val = std::numeric_limits<uint16_t>::max();
  resp.error = connection->update_connection_params(static_cast<uint16_t>(std::min(msg.min_interval, max_val)),
                                                    static_cast<uint16_t>(std::min(msg.max_interval, max_val)),
                                                    static_cast<uint16_t>(std::min(msg.latency, max_val)),
                                                    static_cast<uint16_t>(std::min(msg.timeout, max_val)));
  this->api_connection_->send_message(resp);
}

#endif  // BLUETOOTH_CONNECTION_HAS_GATT

#ifdef USE_ESP32

void BluetoothProxy::bluetooth_scanner_set_mode(bool active) {
  // esp32 only: BLEHub is the concrete tracker here, so these calls reach
  // tracker-native methods beyond the neutral contract.
  if (this->hub_->get_scan_active() == active) {
    return;
  }
  ESP_LOGD(TAG, "Setting scanner mode to %s", active ? "active" : "passive");
  this->hub_->set_scan_active(active);
  this->hub_->stop_scan();
  this->hub_->set_scan_continuous(
      true);  // Set this to true to automatically start scanning again when it has cleaned up.
}

#else  // !USE_ESP32

void BluetoothProxy::bluetooth_scanner_set_mode(bool active) {
  if (this->hub_->scan_active() != active) {
    ESP_LOGD(TAG, "Setting scanner mode to %s", active ? "active" : "passive");
    if (!this->hub_->request_scan_mode(active)) {
      // Passive-only controller asked for active scanning; the state report
      // below carries the real, unchanged mode so the subscriber does not
      // assume the change happened.
      ESP_LOGW(TAG, "Scanner mode %s not supported by this tracker", active ? "active" : "passive");
    }
  }
#ifndef USE_BLE_SCANNER_STATE_CALLBACK
  if (this->api_connection_ != nullptr) {
    // Reports the mode change; the sender also refreshes last_scan_running_, so
    // a failed restart (scan_running_ dropped by the tracker) is not reported
    // again by loop() on the next tick. A push hub reports the restart's
    // transitions (mode rides along) instead.
    this->send_polled_scanner_state_();
  }
#endif
}

#endif  // USE_ESP32

void BluetoothProxy::loop() {
#ifdef BLUETOOTH_CONNECTION_HAS_GATT
  // Stream pending service-discovery batches every iteration; the streamer
  // handles a vanished API connection itself.
  for (uint8_t i = 0; i < this->connection_count_; i++) {
    this->connections_[i]->process_pending_services();
  }
#endif

  // Run advertisement flush / scanner-state poll every 100ms
  uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_advertisement_flush_time_ < 100)
    return;
  this->last_advertisement_flush_time_ = now;

  if (this->connections_free_pending_ && this->api_connection_ != nullptr) {
    // Resend a dropped slot-state update, paced by the 100 ms gate so the
    // retry does not hammer the congestion it exists to survive; the
    // advertisement-only arm answers DISCONNECT requests with this message
    // too, so the drain compiles on every proxy build.
    this->connections_free_pending_ = false;
    this->send_connections_free(this->api_connection_);
  }

  if (!api::global_api_server->is_connected() || this->api_connection_ == nullptr) {
#ifdef BLUETOOTH_CONNECTION_HAS_GATT
    // The API subscriber is gone: tear down any connections it left behind
    // (disconnect() on an already-disconnecting slot is a no-op).
    for (uint8_t i = 0; i < this->connection_count_; i++) {
      auto *connection = this->connections_[i];
      if (connection->get_address() != 0) {
        connection->disconnect();
      }
    }
#endif
    return;
  }

#ifndef USE_BLE_SCANNER_STATE_CALLBACK
  // This hub doesn't push scanner-state transitions; poll and report on
  // change. A hub gaining push emits the define and drops this poll.
  if (this->hub_->scan_running() != this->last_scan_running_) {
    this->send_polled_scanner_state_();
  }
#endif

  this->flush_pending_advertisements_();
}

#ifndef BLUETOOTH_CONNECTION_HAS_GATT

// Advertisement-only proxy. GATT client connections are excluded at compile
// time (no connection backend on this platform, or active: false), so every
// connection-oriented request is answered with a clean error instead of
// silence, and Home Assistant treats the proxy as passive.

void BluetoothProxy::bluetooth_device_request(const api::BluetoothDeviceRequest &msg) {
  switch (msg.request_type) {
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITH_CACHE:
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITHOUT_CACHE:
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT:
      ESP_LOGW(TAG, "Active connections are not supported on this platform");
      this->send_device_connection(msg.address, false, 0, GATT_NOT_CONNECTED);
      break;
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_DISCONNECT:
      // Not an error: the device is already disconnected, which is the requested state.
      this->send_device_connection(msg.address, false);
      this->send_connections_free();
      break;
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_PAIR:
      this->send_device_pairing(msg.address, false, GATT_NOT_CONNECTED);
      break;
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_UNPAIR: {
      // Address-scoped maintenance needs no connection slot: real on esp32
      // (Bluedroid bond table), the stub elsewhere keeps the old error reply.
      conn_err_t ret = bluetooth_connection::unpair_device(msg.address);
      this->send_device_unpairing(msg.address, ret == CONN_OK, ret);
      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CLEAR_CACHE: {
      conn_err_t ret = bluetooth_connection::clear_gatt_cache(msg.address);
      this->send_device_clear_cache(msg.address, ret == CONN_OK, ret);
      break;
    }
  }
}

void BluetoothProxy::bluetooth_gatt_read(const api::BluetoothGATTReadRequest &msg) {
  this->handle_gatt_not_connected_(msg.address, msg.handle, "read", "characteristic");
}

void BluetoothProxy::bluetooth_gatt_write(const api::BluetoothGATTWriteRequest &msg) {
  this->handle_gatt_not_connected_(msg.address, msg.handle, "write", "characteristic");
}

void BluetoothProxy::bluetooth_gatt_read_descriptor(const api::BluetoothGATTReadDescriptorRequest &msg) {
  this->handle_gatt_not_connected_(msg.address, msg.handle, "read", "descriptor");
}

void BluetoothProxy::bluetooth_gatt_write_descriptor(const api::BluetoothGATTWriteDescriptorRequest &msg) {
  this->handle_gatt_not_connected_(msg.address, msg.handle, "write", "descriptor");
}

void BluetoothProxy::bluetooth_gatt_send_services(const api::BluetoothGATTGetServicesRequest &msg) {
  this->handle_gatt_not_connected_(msg.address, 0, "get", "services");
}

void BluetoothProxy::bluetooth_gatt_notify(const api::BluetoothGATTNotifyRequest &msg) {
  this->handle_gatt_not_connected_(msg.address, msg.handle, "notify", "characteristic");
}

void BluetoothProxy::bluetooth_set_connection_params(const api::BluetoothSetConnectionParamsRequest &msg) {
  if (this->api_connection_ == nullptr)
    return;
  // Send results unchecked (esp32 parity): a drop resolves via the client timeout.
  api::BluetoothSetConnectionParamsResponse resp;
  resp.address = msg.address;
  resp.error = GATT_NOT_CONNECTED;
  this->api_connection_->send_message(resp);
}

#endif  // !BLUETOOTH_CONNECTION_HAS_GATT

void BluetoothProxy::subscribe_api_connection(api::APIConnection *api_connection, uint32_t flags) {
  if (this->api_connection_ != nullptr && this->api_connection_ != api_connection) {
    // A previous subscriber still holds the slot. This is almost always a stale
    // connection from a client that dropped without a clean disconnect and has
    // not yet hit the keepalive timeout; rejecting the new subscriber would
    // silently starve it of advertisements until it reconnects, so the newest
    // subscriber wins instead.
    char old_peername[socket::SOCKADDR_STR_LEN];
    char new_peername[socket::SOCKADDR_STR_LEN];
    ESP_LOGW(TAG, "Subscription from %s (%s) replaces %s (%s)", api_connection->get_name(),
             api_connection->get_peername_to(new_peername), this->api_connection_->get_name(),
             this->api_connection_->get_peername_to(old_peername));
  }
  // A stale retry latch belongs to the previous subscriber's session.
  this->connections_free_pending_ = false;
  this->api_connection_ = api_connection;
#ifdef USE_BLE_SCANNER_STATE_CALLBACK
  // get_scanner_state() is part of the push-hub surface (see BLEHubContract).
  this->send_bluetooth_scanner_state_(this->hub_->get_scanner_state());
#else
  this->send_polled_scanner_state_();
#endif
}

void BluetoothProxy::unsubscribe_api_connection(api::APIConnection *api_connection) {
  if (this->api_connection_ != api_connection) {
    ESP_LOGV(TAG, "API connection is not subscribed");
    return;
  }
  this->api_connection_ = nullptr;
  this->connections_free_pending_ = false;
}

void BluetoothProxy::send_device_connection(uint64_t address, bool connected, uint16_t mtu, conn_err_t error) {
  if (this->api_connection_ == nullptr)
    return;
  api::BluetoothDeviceConnectionResponse call;
  call.address = address;
  call.connected = connected;
  call.mtu = mtu;
  call.error = error;
  // Fire and forget: a drop is covered by the client's own timeouts and the
  // retried connections-free state.
  this->api_connection_->send_message(call);
}
void BluetoothProxy::send_connections_free() {
  if (this->api_connection_ != nullptr) {
    this->send_connections_free(this->api_connection_);
  }
}

void BluetoothProxy::send_connections_free(api::APIConnection *api_connection) {
  // Latch only for the current subscriber: loop() resends to api_connection_.
  if (!api_connection->send_message(this->connections_free_response_) && api_connection == this->api_connection_) {
    // V like the api layer's own buffer-full log: a D would ride the same
    // full connection.
    ESP_LOGV(TAG, "Connections-free update deferred, TCP buffer full");
    this->connections_free_pending_ = true;
  }
}

void BluetoothProxy::send_gatt_services_done(uint64_t address) {
  if (this->api_connection_ == nullptr)
    return;
  api::BluetoothGATTGetServicesDoneResponse call;
  call.address = address;
  this->api_connection_->send_message(call);
}

void BluetoothProxy::send_gatt_error(uint64_t address, uint16_t handle, conn_err_t error) {
  if (this->api_connection_ == nullptr)
    return;
  api::BluetoothGATTErrorResponse call;
  call.address = address;
  call.handle = handle;
  call.error = error;
  this->api_connection_->send_message(call);
}

void BluetoothProxy::send_device_pairing(uint64_t address, bool paired, conn_err_t error) {
  if (this->api_connection_ == nullptr)
    return;
  api::BluetoothDevicePairingResponse call;
  call.address = address;
  call.paired = paired;
  call.error = error;

  this->api_connection_->send_message(call);
}

void BluetoothProxy::send_device_unpairing(uint64_t address, bool success, conn_err_t error) {
  if (this->api_connection_ == nullptr)
    return;
  api::BluetoothDeviceUnpairingResponse call;
  call.address = address;
  call.success = success;
  call.error = error;

  this->api_connection_->send_message(call);
}

// Shared by both platform paths: the neutral bluetooth_device_request() uses it to
// answer a clear-cache request with a clean error, so it must not be esp32-guarded.
void BluetoothProxy::send_device_clear_cache(uint64_t address, bool success, conn_err_t error) {
  if (this->api_connection_ == nullptr)
    return;
  api::BluetoothDeviceClearCacheResponse call;
  call.address = address;
  call.success = success;
  call.error = error;

  this->api_connection_->send_message(call);
}

BluetoothProxy *global_bluetooth_proxy = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::bluetooth_proxy

#endif  // USE_BLUETOOTH_PROXY
