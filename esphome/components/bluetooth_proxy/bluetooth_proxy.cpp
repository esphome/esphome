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
    return true;  // Nobody subscribed: nothing owed
  api::BluetoothScannerStateResponse resp;
  resp.state = static_cast<api::enums::BluetoothScannerState>(state);
  resp.mode = this->hub_->scan_active() ? api::enums::BluetoothScannerMode::BLUETOOTH_SCANNER_MODE_ACTIVE
                                        : api::enums::BluetoothScannerMode::BLUETOOTH_SCANNER_MODE_PASSIVE;
  resp.configured_mode = this->configured_scan_active_
                             ? api::enums::BluetoothScannerMode::BLUETOOTH_SCANNER_MODE_ACTIVE
                             : api::enums::BluetoothScannerMode::BLUETOOTH_SCANNER_MODE_PASSIVE;
  return this->api_connection_->send_message(resp);
}

#ifdef USE_BLE_SCANNER_STATE_CALLBACK
void BluetoothProxy::send_scanner_state_(ble_device_base::ScannerState state) {
  // False only on a refused frame, so the latch arms only when a retry is owed.
  this->scanner_state_pending_ = !this->send_bluetooth_scanner_state_(state);
}
#else
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
#endif  // USE_BLE_SCANNER_STATE_CALLBACK

void BluetoothProxy::setup() {
#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
  this->connections_free_response_.limit = BLUETOOTH_PROXY_MAX_CONNECTIONS;
  this->connections_free_response_.free = BLUETOOTH_PROXY_MAX_CONNECTIONS;
#endif

  // Capture the configured scan mode from YAML before any API changes
  this->configured_scan_active_ = this->hub_->scan_active();

  this->hub_->set_raw_advertisement_callback({this, [](void *self, const ble_device_base::RawAdvertisement &adv) {
                                                static_cast<BluetoothProxy *>(self)->on_raw_advertisement_(adv);
                                              }});
#ifdef USE_BLE_SCANNER_STATE_CALLBACK
  // Only push hubs compile the slot; elsewhere loop() polls scan_running().
  this->hub_->set_scanner_state_callback({this, [](void *self, ble_device_base::ScannerState state) {
                                            static_cast<BluetoothProxy *>(self)->send_scanner_state_(state);
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

  ESP_LOGVV(TAG, "Queuing raw packet from %012" PRIX64 ", length %d. RSSI: %d dB", raw.address, length, raw.rssi);

  // Flush if we have reached BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE
  if (this->response_.advertisements_len >= BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE) {
    this->flush_pending_advertisements_();
  }
}

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
void BluetoothProxy::log_connection_request_ignored_(BluetoothConnection *connection, ClientState state) {
  ESP_LOGW(TAG, "[%d] [%s] Connection request ignored, state: %s", connection->get_connection_index(),
           connection->address_str(), ble_device_base::client_state_to_string(state));
}

void BluetoothProxy::log_connection_info_(BluetoothConnection *connection, const char *message) {
  ESP_LOGI(TAG, "[%d] [%s] Connecting %s", connection->get_connection_index(), connection->address_str(), message);
}
#endif  // USE_BLUETOOTH_PROXY_CONNECTIONS

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
void BluetoothProxy::log_reply_dropped_(const char *what, uint64_t address) {
  ESP_LOGW(TAG, "%s reply for %012" PRIX64 " dropped, TCP buffer full", what, address);
}

void BluetoothProxy::log_reply_deferred_(const char *what, uint64_t address) {
  ESP_LOGW(TAG, "%s reply for %012" PRIX64 " deferred, TCP buffer full", what, address);
}

void BluetoothProxy::log_reply_displaced_(const char *what, uint64_t owed, uint64_t address) {
  ESP_LOGW(TAG, "%s reply for %012" PRIX64 " dropped, displaced by %012" PRIX64, what, owed, address);
}

void BluetoothProxy::log_not_connected_gatt_(const char *action, const char *type) {
  ESP_LOGW(TAG, "Cannot %s GATT %s, not connected", action, type);
}

void BluetoothProxy::handle_gatt_not_connected_(uint64_t address, uint16_t handle, const char *action,
                                                const char *type) {
  this->log_not_connected_gatt_(action, type);
  if (!this->send_gatt_error(address, handle, GATT_NOT_CONNECTED)) {
    // No connection, so nothing to latch against; the client's timeout arbitrates.
    this->log_reply_dropped_("Not-connected", address);
  }
}
#endif

void BluetoothProxy::log_advertisement_flush_(bool sent) {
  if (sent) {
    // VV: one line per flush drowns a verbose log in any busy environment.
    ESP_LOGVV(TAG, "Sent batch of %u BLE advertisements", this->response_.advertisements_len);
  } else {
    // The rare congestion signal stays at V.
    ESP_LOGV(TAG, "Batch of %u BLE advertisements dropped, TCP buffer full", this->response_.advertisements_len);
  }
}

void BluetoothProxy::dump_config() {
  // Print configured facts. dump_config runs right after setup, before the
  // radio is up, so live scan state would always read "stopped" here — the
  // loop's BluetoothScannerStateResponse carries the changing value instead.
  char mac_str[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  this->get_bluetooth_mac_address_pretty(mac_str);
  const char *mac_out = mac_str[0] != '\0' ? mac_str : "unavailable (adapter not up yet)";
  const char *scan_mode = this->configured_scan_active_ ? "active" : "passive";
#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
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

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS

void BluetoothProxy::register_connection(BluetoothConnection *connection) {
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

void BluetoothProxy::latch_pending_disconnection_(uint64_t address, conn_err_t error) {
  // Match before free entry so one address never occupies two pool slots.
  PendingReply *free_entry = nullptr;
  for (uint8_t i = 0; i < this->connection_count_; i++) {
    auto &owed = this->pending_disconnections_[i];
    if (owed.matches(address)) {
      owed.set(address, error);
      return;
    }
    if (free_entry == nullptr && owed.empty()) {
      free_entry = &owed;
    }
  }
  if (free_entry != nullptr) {
    this->log_reply_deferred_("Disconnect", address);
    free_entry->set(address, error);
    return;
  }
  // Every entry is owed: evict the first so the newest loss is not silent too.
  this->log_reply_displaced_("Disconnect", this->pending_disconnections_[0].address(), address);
  this->pending_disconnections_[0].set(address, error);
}

void BluetoothProxy::clear_pending_disconnection_(uint64_t address) {
  // A reconnect supersedes the owed disconnect; a late resend would shadow
  // the new connection.
  for (uint8_t i = 0; i < this->connection_count_; i++) {
    if (this->pending_disconnections_[i].matches(address)) {
      this->pending_disconnections_[i].clear();
      return;  // latch_pending_disconnection_ keeps at most one entry per address
    }
  }
}

void BluetoothProxy::answer_device_disconnected_(uint64_t address) {
  if (this->send_device_connection(address, false)) {
    // A landed answer satisfies any owed notification for the address; a
    // drained duplicate would follow it otherwise.
    this->clear_pending_disconnection_(address);
    return;
  }
  // Not latched: the client's own request timeout arbitrates, and pooling
  // these would let a request retry loop displace an unsolicited disconnect.
  this->log_reply_dropped_("Disconnect", address);
}

void BluetoothProxy::send_device_disconnected_(uint64_t address, conn_err_t error) {
  if (this->send_device_connection(address, false, 0, error)) {
    // A later disconnect landing for an address that still has one owed would
    // otherwise have the drain repeat it.
    this->clear_pending_disconnection_(address);
    return;
  }
  // A dropped disconnect leaves the client believing the link is live, so
  // every GATT operation on it times out until something else corrects it.
  // latch_pending_disconnection_() reports the leading edge.
  this->latch_pending_disconnection_(address, error);
}

void BluetoothProxy::reset_connection_slot_(BluetoothConnection *connection, conn_err_t reason) {
  // The client has no other way to learn of an unsolicited disconnect.
  this->send_device_disconnected_(connection->get_address(), reason);
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

    if (conn_addr == address) {
      // A connect request supersedes an owed disconnect.
      if (reserve) {
        this->clear_pending_disconnection_(address);
      }
      return connection;
    }

    if (free_slot == nullptr && conn_addr == 0)
      free_slot = connection;
  }
  if (!reserve || free_slot == nullptr)
    return nullptr;
  this->clear_pending_disconnection_(address);
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
        this->answer_device_disconnected_(msg.address);
        return;
      }
      if (!msg.has_address_type) {
        ESP_LOGE(TAG, "[%d] [%s] Missing address type in connect request", connection->get_connection_index(),
                 connection->address_str());
        this->answer_device_disconnected_(msg.address);
        return;
      }
      if (connection->state() == ClientState::CONNECTED || connection->state() == ClientState::ESTABLISHED) {
        this->log_connection_request_ignored_(connection, connection->state());
        connection->send_connected_reply_();
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
        this->answer_device_disconnected_(msg.address);
        this->send_connections_free();
        return;
      }
      if (connection->state() != ClientState::IDLE) {
        connection->disconnect();
      } else {
        connection->set_address(0);
        this->answer_device_disconnected_(msg.address);
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
      this->answer_device_disconnected_(msg.address);
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
    connection->send_gatt_error_(msg.handle, err);
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
    connection->send_gatt_error_(msg.handle, err);
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
    connection->send_gatt_error_(msg.handle, err);
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
    connection->send_gatt_error_(msg.handle, err);
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
    // Through the retrying sender: a drop must not leave discovery hanging.
    // Re-entry does not depend on the cursor - this branch is gated on
    // has_gatt_services() alone, so no restore is needed.
    connection->send_services_done_();
    return;
  }
  if (connection->send_service_ > 0) {
    // A request mid-stream restarts from the top so the requester always
    // gets the full list. No duplicate risk: the client accumulates batches
    // per request, and a same-session re-request only happens after the
    // previous request timed out and discarded its partial list.
    ESP_LOGD(TAG, "[%d] [%s] GetServices mid-stream, restarting", connection->get_connection_index(),
             connection->address_str());
    connection->send_service_ = 0;
    return;
  }
  if (connection->send_service_ == SERVICES_DONE_PENDING) {
    // A new request supersedes an owed done: the client accumulates batches
    // per request, so its fresh, empty accumulator plus a bare done would
    // cache as an empty database. The table is freed; the client's timeout
    // arbitrates.
    ESP_LOGW(TAG, "[%d] [%s] GetServices superseded an undelivered done; client timeout will retry",
             connection->get_connection_index(), connection->address_str());
    connection->send_service_ = DONE_SENDING_SERVICES;
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
    connection->send_gatt_error_(msg.handle, err);
  }
}

void BluetoothProxy::bluetooth_set_connection_params(const api::BluetoothSetConnectionParamsRequest &msg) {
  if (this->api_connection_ == nullptr)
    return;
  // Not latched (esp32 parity): the request is idempotent, so a drop resolves
  // via the client timeout and a retry gives the same answer. Still reported.

  auto *connection = this->get_connection_(msg.address, false);
  api::BluetoothSetConnectionParamsResponse resp;
  resp.address = msg.address;

  if (connection == nullptr || !connection->connected()) {
    ESP_LOGW(TAG, "[%d] [%s] Cannot set connection params, not connected",
             connection ? static_cast<int>(connection->get_connection_index()) : -1,
             connection ? connection->address_str() : "unknown");
    resp.error = GATT_NOT_CONNECTED;
    if (!this->api_connection_->send_message(resp)) {
      this->log_reply_dropped_("Connection-params", msg.address);
    }
    return;
  }

  // Protobuf fields are uint32_t to future-proof the API if BLE ever supports wider values;
  // clamp to uint16_t since the current BLE spec defines these as 16-bit.
  constexpr uint32_t max_val = std::numeric_limits<uint16_t>::max();
  resp.error = connection->update_connection_params(static_cast<uint16_t>(std::min(msg.min_interval, max_val)),
                                                    static_cast<uint16_t>(std::min(msg.max_interval, max_val)),
                                                    static_cast<uint16_t>(std::min(msg.latency, max_val)),
                                                    static_cast<uint16_t>(std::min(msg.timeout, max_val)));
  if (!this->api_connection_->send_message(resp)) {
    this->log_reply_dropped_("Connection-params", msg.address);
  }
}

#endif  // USE_BLUETOOTH_PROXY_CONNECTIONS

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
#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
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

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
  if (this->connections_free_pending_ && this->api_connection_ != nullptr) {
    // Resend a dropped slot-state update, paced by the 100 ms gate so the
    // retry does not hammer the congestion it exists to survive. Every build
    // sends this at subscribe time (api_connection.cpp), so the drain
    // compiles on every proxy build.
    this->connections_free_pending_ = false;
    this->send_connections_free(this->api_connection_);
  }
#endif

  if (!api::global_api_server->is_connected() || this->api_connection_ == nullptr) {
#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
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

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
  // Paced retries of owed per-slot notifications; subscriber swaps clear
  // stale latches before this runs.
  for (uint8_t i = 0; i < this->connection_count_; i++) {
    this->connections_[i]->flush_owed_replies_();
  }
  // Address-keyed, not slot-keyed, so it gets its own loop; bounded by
  // connection_count_ like the latch and clear helpers. Not pre-cleared:
  // the sender clears on success and re-latches on refusal, keeping the
  // latch's leading-edge warn honest (same shape as the unpair drain).
  for (uint8_t i = 0; i < this->connection_count_; i++) {
    auto &owed = this->pending_disconnections_[i];
    if (owed.empty())
      continue;
    this->send_device_disconnected_(owed.address(), owed.error());
  }

  // An owed unpair reply. Not pre-cleared: the sender clears on success and
  // re-latches on refusal, keeping its leading-edge warn guard honest.
  if (!this->pending_unpairing_.empty()) {
    conn_err_t error = this->pending_unpairing_.error();
    this->send_device_unpairing(this->pending_unpairing_.address(), error == CONN_OK, error);
  }
#endif

#ifdef USE_BLE_SCANNER_STATE_CALLBACK
  // Resend a dropped scanner-state push (see scanner_state_pending_).
  if (this->scanner_state_pending_) {
    this->send_scanner_state_(this->hub_->get_scanner_state());
  }
#else
  // This hub doesn't push scanner-state transitions; poll and report on
  // change. A hub gaining push emits the define and drops this poll.
  if (this->hub_->scan_running() != this->last_scan_running_) {
    this->send_polled_scanner_state_();
  }
#endif

#ifdef USE_WIFI
  // Wi-Fi (or a coexistence build that can fall back to it): every other
  // non-empty 100 ms tick (~200 ms) gives partial batches time to fill
  // toward BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE, so the air gets fewer,
  // fuller frames. Full batches still ship immediately from the queueing
  // path, and the owed-reply drains above keep the 100 ms cadence.
  if (this->response_.advertisements_len != 0) {
    if (this->adv_flush_toggle_) {
      this->flush_pending_advertisements_();
    }
    this->adv_flush_toggle_ = !this->adv_flush_toggle_;
  } else {
    // Nothing pending (idle, or a full batch just shipped inline): arm so
    // the next batch ships on the next tick.
    this->adv_flush_toggle_ = true;
  }
#else
  // No Wi-Fi in the build (ethernet): no airtime worth trading latency for,
  // so partial batches flush every tick.
  this->flush_pending_advertisements_();
#endif
}

void BluetoothProxy::reset_owed_replies_() {
#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
  this->connections_free_pending_ = false;
#endif
#ifdef USE_BLE_SCANNER_STATE_CALLBACK
  // Owed on unsubscribe; on subscribe the trailing send_scanner_state_()
  // re-drives it from the hub, so clearing it there is free.
  this->scanner_state_pending_ = false;
#else
  // Force a poll-arm mismatch: a frame refused at subscribe time could
  // otherwise match the stale detector and never be retried. Inert on
  // unsubscribe: loop() returns at the no-subscriber gate before the
  // detector runs, and a re-subscribe re-arms this anyway.
  this->last_scan_running_ = !this->hub_->scan_running();
#endif
#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
  this->pending_unpairing_.clear();
  this->pending_disconnections_.fill({});
  for (uint8_t i = 0; i < this->connection_count_; i++) {
    // Neither a partial stream's tail nor an owed done belongs to the next
    // session; silence (the client's timeout) arbitrates.
    auto *connection = this->connections_[i];
    connection->park_service_stream_();
    connection->clear_owed_flags_();
  }
#endif
}

void BluetoothProxy::subscribe_api_connection(api::APIConnection *api_connection, uint32_t flags) {
  if (api_connection != this->api_connection_) {
    if (this->api_connection_ != nullptr) {
      // A previous subscriber still holds the slot. This is almost always a
      // stale connection from a client that dropped without a clean disconnect
      // and has not yet hit the keepalive timeout; rejecting the new
      // subscriber would silently starve it of advertisements until it
      // reconnects, so the newest subscriber wins instead.
      char old_peername[socket::SOCKADDR_STR_LEN];
      char new_peername[socket::SOCKADDR_STR_LEN];
      ESP_LOGW(TAG, "Subscription from %s (%s) replaces %s (%s)", api_connection->get_name(),
               api_connection->get_peername_to(new_peername), this->api_connection_->get_name(),
               this->api_connection_->get_peername_to(old_peername));
    }
    // Stale retry latches belong to the previous subscriber's session; a
    // re-subscribe by the current one keeps what it is still owed.
    this->reset_owed_replies_();
  }
  this->api_connection_ = api_connection;
#ifdef USE_BLE_SCANNER_STATE_CALLBACK
  // get_scanner_state() is part of the push-hub surface (see BLEHubContract).
  this->send_scanner_state_(this->hub_->get_scanner_state());
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
  this->reset_owed_replies_();
}

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
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

bool BluetoothProxy::send_device_connection(uint64_t address, bool connected, uint16_t mtu, conn_err_t error) {
  if (this->api_connection_ == nullptr)
    return true;  // Nobody subscribed: nothing owed
  api::BluetoothDeviceConnectionResponse call;
  call.address = address;
  call.connected = connected;
  call.mtu = mtu;
  call.error = error;
  return this->api_connection_->send_message(call);
}

bool BluetoothProxy::send_gatt_services_done(uint64_t address) {
  if (this->api_connection_ == nullptr)
    return true;  // Nobody subscribed: nothing is owed, only a refused frame reports false
  api::BluetoothGATTGetServicesDoneResponse call;
  call.address = address;
  return this->api_connection_->send_message(call);
}

bool BluetoothProxy::send_gatt_error(uint64_t address, uint16_t handle, conn_err_t error) {
  if (this->api_connection_ == nullptr)
    return true;  // Nobody subscribed: nothing is owed, only a refused frame reports false
  api::BluetoothGATTErrorResponse call;
  call.address = address;
  call.handle = handle;
  call.error = error;
  return this->api_connection_->send_message(call);
}

void BluetoothProxy::send_device_pairing(uint64_t address, bool paired, conn_err_t error) {
  if (this->api_connection_ == nullptr)
    return;
  api::BluetoothDevicePairingResponse call;
  call.address = address;
  call.paired = paired;
  call.error = error;

  if (!this->api_connection_->send_message(call)) {
    // Not latched: a retried PAIR is answered from is_paired(), so the client
    // recovers on its own. Still worth saying it happened.
    this->log_reply_dropped_("Pairing", address);
  }
}

void BluetoothProxy::send_device_unpairing(uint64_t address, bool success, conn_err_t error) {
  if (this->api_connection_ == nullptr)
    return;
  // An owed success is the authoritative answer: a later attempt for the
  // same address fails only because the first already removed the bond.
  if (!this->pending_unpairing_.empty() && this->pending_unpairing_.matches(address) &&
      this->pending_unpairing_.error() == CONN_OK) {
    success = true;
    error = CONN_OK;
  }
  api::BluetoothDeviceUnpairingResponse call;
  call.address = address;
  call.success = success;
  call.error = error;

  if (this->api_connection_->send_message(call)) {
    // A later unpair landing for an address that still has one owed would
    // otherwise have the drain repeat it.
    if (this->pending_unpairing_.matches(address)) {
      this->pending_unpairing_.clear();
    }
    return;
  }
  if (this->pending_unpairing_.empty()) {
    this->log_reply_deferred_("Unpair", address);
  } else if (!this->pending_unpairing_.matches(address)) {
    this->log_reply_displaced_("Unpair", this->pending_unpairing_.address(), address);
  }
  this->pending_unpairing_.set(address, error);
}

// GATT arm only: the advertisement-only arm no longer dispatches CLEAR_CACHE,
// so its response encoder would be dead weight there.
void BluetoothProxy::send_device_clear_cache(uint64_t address, bool success, conn_err_t error) {
  if (this->api_connection_ == nullptr)
    return;
  api::BluetoothDeviceClearCacheResponse call;
  call.address = address;
  call.success = success;
  call.error = error;

  if (!this->api_connection_->send_message(call)) {
    // Not latched: clear-cache is idempotent, so a retry gives the same answer.
    this->log_reply_dropped_("Clear-cache", address);
  }
}
#endif  // USE_BLUETOOTH_PROXY_CONNECTIONS

BluetoothProxy *global_bluetooth_proxy = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::bluetooth_proxy

#endif  // USE_BLUETOOTH_PROXY
