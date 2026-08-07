// Hub-platform BluetoothConnection: drives a platform GATT client backend
// through the neutral ble_device_base::BLEGattConnection interface and
// translates its events into the same API messages the esp32 class emits.
// Presents the identical method surface, so the proxy's GATT dispatch
// compiles against either class unchanged.

#pragma once

#include "esphome/core/defines.h"

#if !defined(USE_ESP32) && defined(USE_BLE_GATT_CLIENT)

#include "bluetooth_connection.h"

#include "esphome/components/ble_device_base/ble_client_state.h"
#include "esphome/components/ble_device_base/ble_gatt_client.h"
#include "esphome/core/helpers.h"

namespace esphome::bluetooth_proxy {
class BluetoothProxy;
}  // namespace esphome::bluetooth_proxy

namespace esphome::bluetooth_connection {

using ClientState = ble_device_base::ClientState;
using ConnectionType = ble_device_base::ConnectionType;

class BluetoothConnection final : public ble_device_base::GattClientEventListener {
 public:
  /// Wire the platform backend. Called from codegen before setup.
  void set_backend(ble_device_base::BLEGattConnection *backend) {
    this->backend_ = backend;
    backend->set_listener(this);
  }

  // ---- proxy dispatch surface (mirrors the esp32 class) ----
  conn_err_t read_characteristic(uint16_t handle);
  conn_err_t write_characteristic(uint16_t handle, const uint8_t *data, size_t length, bool response);
  conn_err_t read_descriptor(uint16_t handle);
  conn_err_t write_descriptor(uint16_t handle, const uint8_t *data, size_t length, bool response);
  conn_err_t notify_characteristic(uint16_t handle, bool enable);
  conn_err_t update_connection_params(uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout);

  /// Start connecting: record the API address type (BLE_ADDR_TYPE_* code
  /// space) and open the connection through the backend. Failures report
  /// through the same reset path a failed open takes on esp32.
  void initiate_connection(uint8_t address_type) {
    this->remote_addr_type_ = address_type;
    this->start_connect_();
  }
  void disconnect();
  // A backend disconnect() is a single call that also cancels an in-progress
  // connect; there is no deferred-disconnect state to track.
  bool disconnect_pending() const { return false; }
  void cancel_pending_disconnect() {}

  void set_address(uint64_t address);
  uint64_t get_address() const { return this->address_; }
  const char *address_str() const { return this->address_str_; }
  uint8_t get_connection_index() const { return this->connection_index_; }

  ClientState state() const { return this->state_; }
  void set_state(ClientState st) { this->state_ = st; }
  bool connected() const { return this->state_ == ClientState::ESTABLISHED; }
  void set_connection_type(ConnectionType ct) { this->connection_type_ = ct; }
  // Latched at discovery completion rather than read from the backend table:
  // streaming frees the table, and this must stay true for the connection's
  // lifetime (esp32 parity — a repeat GetServices is silently ignored there,
  // never answered with an authoritative empty database).
  bool has_gatt_services() const { return this->services_discovered_; }

  /// Stream any pending service-discovery batch and police the disconnect
  /// safety timeout. Called from the proxy's loop — hub connections have no
  /// Component loop of their own (the esp32 class streams from its own
  /// loop() and has the same 10 s safety net in its base class).
  void process_pending_services() {
    if (this->send_service_ >= 0) {
      this->send_service_for_discovery_();
    }
    this->check_disconnect_timeout_();
  }

  // ---- ble_device_base::GattClientEventListener ----
  void on_connection_state(bool connected, uint16_t mtu, int error) override;
  void on_service_discovery_done(int error) override;
  void on_read_result(uint16_t handle, const uint8_t *data, uint16_t len, int error) override;
  void on_write_result(uint16_t handle, int error) override;
  void on_notify_state(uint16_t handle, bool enabled, int error) override;
  void on_notify_data(uint16_t handle, const uint8_t *data, uint16_t len) override;

 protected:
  friend class bluetooth_proxy::BluetoothProxy;

  void start_connect_();
  void send_service_for_discovery_();
  void check_disconnect_timeout_();
  void reset_connection_(conn_err_t reason);
  conn_err_t check_connected_op_(const char *action, const char *type) const;
  void log_gatt_operation_error_(const char *operation, uint16_t handle, int status);

  // Memory optimized layout for 32-bit systems (a vptr precedes: pointers and
  // 2-byte members first fill to an 8-byte boundary before address_)
  // Group 1: Pointers (4 bytes each, naturally aligned)
  bluetooth_proxy::BluetoothProxy *proxy_{nullptr};
  ble_device_base::BLEGattConnection *backend_{nullptr};

  // Group 2: 2-byte types
  int16_t send_service_{INIT_SENDING_SERVICES};
  uint16_t mtu_{23};

  // Group 3: 8-byte and 4-byte types
  uint64_t address_{0};
  uint32_t disconnecting_started_{0};
  conn_err_t pending_error_{0};

  // Group 4: Arrays
  char address_str_[MAC_ADDRESS_PRETTY_BUFFER_SIZE]{};

  // Group 5: 1-byte types
  ClientState state_{ClientState::IDLE};
  ConnectionType connection_type_{ConnectionType::V1};
  uint8_t remote_addr_type_{0};
  uint8_t connection_index_{0};
  bool services_discovered_{false};
};

}  // namespace esphome::bluetooth_connection

#endif  // !USE_ESP32 && USE_BLE_GATT_CLIENT
