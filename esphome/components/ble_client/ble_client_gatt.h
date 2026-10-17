// Platform-neutral ble_client engine on the ble_device_base GATT contract.
//
// Compiled on every platform with a GATT backend except esp32, which keeps
// the legacy BLEClientBase engine (ble_client.h) until its raw-gattc node
// family migrates - the exclusive gates make the same class names resolve to
// exactly one definition per build, so codegen is shared.
//
// Connects are sighting-gated like the legacy engine: the client is a parsed
// advertisement listener, captures the peer's address type from the sighting,
// and asks the backend to connect only when enabled and idle.

#pragma once

#include "esphome/core/defines.h"

#if defined(USE_BLE_GATT_CLIENT) && !defined(USE_BLE_CLIENT_LEGACY_ENGINE)

#include "ble_client_node.h"
#include "connect_backoff.h"
#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/ble_device_base/ble_gatt_client.h"
#include "esphome/components/bluetooth_connection/bluetooth_connection.h"
#include "esphome/components/bluetooth_connection/bluetooth_connection_gatt_backend.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <cstdint>
#include <functional>

namespace esphome::ble_client {

class BLEClient : public Component,
                  public ble_device_base::ESPBTDeviceListener,
                  public ble_device_base::GattClientListener {
 public:
  void dump_config() override;

  // Public field for legacy parity (the switch platform republishes it).
  bool enabled{true};

  void set_backend(ble_device_base::BLEGattConnection *backend) {
    this->backend_ = backend;
    backend->set_listener(this);
  }
  void set_address(uint64_t address);
  void set_auto_connect(bool auto_connect) { this->auto_connect_ = auto_connect; }
  void set_enabled(bool enabled);
  const char *address_str() const { return this->address_str_; }

  void register_ble_node(BLEClientNode *node);
  // One registration spelling shared with the esp32 engine's bridge.
  void register_gatt_node(BLEClientNode *node) { this->register_ble_node(node); }

  bool connected() const { return this->state_ == State::CONNECTED; }
  bool idle() const { return this->state_ == State::IDLE; }

  /// Action-initiated connect (no sighting needed; uses the last captured
  /// address type, public until a sighting arrives). No-op unless idle.
  void connect();
  void disconnect();

  /// Legacy-named deferral used by the automation twins: neutral listener
  /// callbacks run inside the backend's event drain, so automation chain
  /// continuations must leave that stack first.
  void run_later(std::function<void()> &&f) { this->defer(std::move(f)); }  // NOLINT

  // Backend ops for nodes and actions - the frozen node-facing surface.
  // Only write_characteristic has an in-tree caller; subscribing means
  // notify_characteristic plus a CCCD write_descriptor (the caller's job
  // per the contract).
  int write_characteristic(uint16_t handle, const uint8_t *data, uint16_t len, bool response) {
    return this->backend_->write_characteristic(handle, data, len, response);
  }
  int read_characteristic(uint16_t handle) { return this->backend_->read_characteristic(handle); }
  int read_descriptor(uint16_t handle) { return this->backend_->read_descriptor(handle); }
  int write_descriptor(uint16_t handle, const uint8_t *data, uint16_t len) {
    return this->backend_->write_descriptor(handle, data, len);
  }
  int notify_characteristic(uint16_t handle, bool enable) {
    return this->backend_->notify_characteristic(handle, enable);
  }
  int pair() { return this->backend_->pair(); }
  int unpair() { return bluetooth_connection::unpair_device(this->address_); }

  // Automation callback registration.
  template<typename F> void add_on_connect_callback(F &&callback) {
    this->connect_callbacks_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_disconnect_callback(F &&callback) {
    this->disconnect_callbacks_.add(std::forward<F>(callback));
  }
  // Fired when a connect attempt dies before being established; the user
  // on_disconnect trigger deliberately does NOT fire here (legacy parity).
  template<typename F> void add_on_connect_failed_callback(F &&callback) {
    this->connect_failed_callbacks_.add(std::forward<F>(callback));
  }

  // ---- ble_device_base::ESPBTDeviceListener ----
  bool parse_device(const ble_device_base::ESPBTDevice &device) override;

  // ---- ble_device_base::GattClientListener ----
  void on_connection_state(bool connected, uint16_t mtu, int error) override;
  void on_service_discovery_done(int error) override;
  void on_read_result(uint16_t handle, const uint8_t *data, uint16_t len, int error) override;
  void on_write_result(uint16_t handle, int error) override;
  void on_notify_data(uint16_t handle, const uint8_t *data, uint16_t len) override;
  void on_notify_state(uint16_t handle, bool enabled, int error) override;
  void on_pairing_result(int status) override;

 protected:
  enum class State : uint8_t { IDLE, CONNECTING, DISCOVERING, CONNECTED };

  void attempt_connect_();

  // Group 1: pointers / containers
  ble_device_base::BLEGattConnection *backend_{nullptr};
  // Codegen-sized (ESPHOME_BLE_CLIENT_MAX_NODES); filled during setup.
  StaticVector<BLEClientNode *, ESPHOME_BLE_CLIENT_MAX_NODES> nodes_;

  // Group 2: 8-byte types
  uint64_t address_{0};

  // Group 3: callback managers (pointer-sized when empty)
  LazyCallbackManager<void()> connect_callbacks_;
  LazyCallbackManager<void()> disconnect_callbacks_;
  LazyCallbackManager<void()> connect_failed_callbacks_;

  // Group 4: 4-byte types
  // Backoff so an undiscoverable database or a dead peer cannot produce a
  // battery-draining connect loop.
  ConnectBackoff backoff_;

  // Group 5: arrays
  char address_str_[MAC_ADDRESS_PRETTY_BUFFER_SIZE]{};

  // Group 6: 1-byte types
  State state_{State::IDLE};
  uint8_t address_type_{0};  // BLE_ADDR_TYPE_*, captured from the sighting
  // Distinguishes a captured public type from the never-sighted default.
  bool address_type_known_{false};
  bool auto_connect_{true};
  // A user-initiated teardown in flight; its failure report is not a
  // connect failure and must not feed the backoff.
  bool cancel_requested_{false};
};

}  // namespace esphome::ble_client

#endif  // USE_BLE_GATT_CLIENT && !USE_BLE_CLIENT_LEGACY_ENGINE
