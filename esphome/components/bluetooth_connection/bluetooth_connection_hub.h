// BluetoothConnection: drives the build's GATT backend (the
// ble_device_base::BLEGattConnection alias) and translates its events into
// the proxy's API messages. One wrapper for every platform; per-backend
// differences live behind the alias and the streamer cut-through.

#pragma once

#include "bluetooth_connection.h"

// The wrapper exists to serve the proxy's API surface; direct consumers
// drive the backend themselves, so backend-only builds compile this header
// empty.
#ifdef BLUETOOTH_CONNECTION_HAS_GATT

#include "esphome/components/ble_device_base/ble_client_state.h"
#include "bluetooth_connection_gatt_backend.h"
#include "esphome/core/helpers.h"

namespace esphome::bluetooth_proxy {
class BluetoothProxy;
}  // namespace esphome::bluetooth_proxy

namespace esphome::bluetooth_connection {

using ClientState = ble_device_base::ClientState;
using ConnectionType = ble_device_base::ConnectionType;

class BluetoothConnection final : public ble_device_base::GattClientListener {
 public:
  /// Wire the platform backend. Called from codegen before setup.
  void set_backend(ble_device_base::BLEGattConnection *backend) {
    this->backend_ = backend;
    backend->set_listener(this);
  }

  // ---- proxy dispatch surface ----
  conn_err_t read_characteristic(uint16_t handle);
  conn_err_t write_characteristic(uint16_t handle, const uint8_t *data, size_t length, bool response);
  conn_err_t read_descriptor(uint16_t handle);
  conn_err_t write_descriptor(uint16_t handle, const uint8_t *data, size_t length, bool response);
  conn_err_t notify_characteristic(uint16_t handle, bool enable);
  conn_err_t update_connection_params(uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout);

  /// Streamer abort: latch the GATT cause, park the cursor, tear down.
  void abort_service_stream(conn_err_t err) {
    this->latch_pending_error_(err);
    this->send_service_ = DONE_SENDING_SERVICES;
    this->disconnect();
  }

  /// Start connecting with the API address type (BLE_ADDR_TYPE_* code
  /// space). Failures report through the same reset path a failed open
  /// takes.
  void initiate_connection(uint8_t address_type);
  void disconnect();
  /// A connect request racing a scheduled teardown: true when the backend
  /// had not started closing - the in-flight open resumes and reports
  /// connected. False once the teardown owns the link.
  bool cancel_teardown() {
    if (this->state_ == ClientState::DISCONNECTING && this->backend_->cancel_gatt_disconnect()) {
      this->state_ = ClientState::CONNECTING;
      return true;
    }
    return false;
  }
  bool is_paired() const { return this->paired_; }
  void set_unpaired() { this->paired_ = false; }
  conn_err_t pair() { return this->backend_->pair(); }

  void set_address(uint64_t address);
  uint64_t get_address() const { return this->address_; }
  const char *address_str() const { return this->address_str_; }
  uint8_t get_connection_index() const { return this->connection_index_; }

  ClientState state() const { return this->state_; }
  void set_state(ClientState st) { this->state_ = st; }
  bool connected() const { return this->state_ == ClientState::ESTABLISHED; }
  void set_connection_type(ConnectionType ct) {
    this->connection_type_ = ct;
    // The bluedroid backend branches on the type itself (prefer-params and
    // the with-cache report at OPEN_EVT); the others ignore it.
    this->backend_->set_connection_type(ct);
  }
  // Latched at discovery completion rather than read from the backend table:
  // streaming frees the table, and this must stay true for the connection's
  // lifetime (a repeat GetServices is silently ignored, never answered with
  // an authoritative empty database).
  bool has_gatt_services() const { return this->services_discovered_; }

  /// Stream any pending service-discovery batch (proxy loop; the backend
  /// owns the disconnect safety timer).
  void process_pending_services() {
    if (this->send_service_ >= 0) {
      this->stream_pending_(this->backend_);
    }
  }

  // ---- backend event listener (called directly by the backend, main loop) ----
  void on_connection_state(bool connected, uint16_t mtu, int error) override;
  void on_service_discovery_done(int error) override;
  void on_read_result(uint16_t handle, const uint8_t *data, uint16_t len, int error) override;
  void on_write_result(uint16_t handle, int error) override;
  void on_notify_state(uint16_t handle, bool enabled, int error) override;
  void on_notify_data(uint16_t handle, const uint8_t *data, uint16_t len) override;
  void on_pairing_result(int status) override;

 protected:
  friend class bluetooth_proxy::BluetoothProxy;
  // The Bluedroid backend streams services in place from its stack cache.
  friend class BluedroidGattClient;

  /// First cause wins: a later, less specific error must not overwrite it.
  void latch_pending_error_(conn_err_t err) {
    if (this->pending_error_ == 0) {
      this->pending_error_ = err;
    }
  }
  // A backend providing its own streamer (see the contract doc) builds the
  // response in place from its stack cache; the rest use the table streamer.
  // Template so the discarded branch is not odr-checked against backends
  // that lack the method.
  template<typename Backend> void stream_pending_(Backend *backend) {
    if constexpr (requires { backend->stream_service_batch(*this); }) {
      backend->stream_service_batch(*this);
    } else {
      this->send_service_for_discovery_();
    }
  }
  void send_service_for_discovery_();
  void reset_connection_(conn_err_t reason);
  conn_err_t check_connected_op_(const char *action, const char *type) const;
  void log_gatt_operation_error_(const char *operation, uint16_t handle, int status);

  // Memory optimized layout for 32-bit systems
  // Group 1: Pointers (4 bytes each, naturally aligned)
  bluetooth_proxy::BluetoothProxy *proxy_{nullptr};
  ble_device_base::BLEGattConnection *backend_{nullptr};

  // Group 2: 2-byte types
  int16_t send_service_{INIT_SENDING_SERVICES};
  uint16_t mtu_{ble_device_base::DEFAULT_ATT_MTU};

  // Group 3: 8-byte and 4-byte types
  uint64_t address_{0};
  conn_err_t pending_error_{0};

  // Group 4: Arrays
  char address_str_[MAC_ADDRESS_PRETTY_BUFFER_SIZE]{};

  // Group 5: bit-packed tail; within 2 bytes the 8-aligned object stays 48.
  static_assert(static_cast<uint8_t>(ClientState::ESTABLISHED) < (1 << 3), "state_ bitfield too narrow");
  static_assert(static_cast<uint8_t>(ConnectionType::V3_WITHOUT_CACHE) < (1 << 2),
                "connection_type_ bitfield too narrow");
  ClientState state_ : 3 {ClientState::IDLE};
  bool paired_ : 1 {false};
  ConnectionType connection_type_ : 2 {ConnectionType::V1};
  uint8_t connection_index_ : 4 {0};
  bool services_discovered_ : 1 {false};
};

}  // namespace esphome::bluetooth_connection

#endif  // BLUETOOTH_CONNECTION_HAS_GATT
