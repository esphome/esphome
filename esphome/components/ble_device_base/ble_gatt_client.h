// ble_gatt_client.h
//
// Platform-neutral GATT client connection contract.
//
// Exactly one GATT backend exists per build, so BLEGattConnection is a
// compile-time alias (bluetooth_connection_gatt_backend.h), not an abstract
// interface.
// A consumer - the hub wrapper streaming the raw database, or a direct
// consumer owning a dedicated backend and resolving handles by UUID -
// drives it and receives completions through the GattClientListener
// interface. All listener calls are delivered on the ESPHome main loop;
// borrowed data pointers are valid only for the duration of the call.
//
// Error domain (plain int, forwarded to the API without translation):
//   0            success
//   1..0x11      ATT error codes (Bluetooth spec; BTstack and Bluedroid agree)
//   GATT_ERR_NOT_CONNECTED (-1)  no connection to the peer (on esp32 a raw
//                ESP_FAIL from the stack shares this value; both read as a
//                failed, unusable connection on the client side)
//   GATT_ERR_NO_MEMORY (-2)      backend storage exhausted
//   anything else: platform stack error/status code, surfaced opaquely.
// Connection events carry HCI status/disconnect reason codes (same code
// space on every controller).

#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BLE_GATT_CLIENT

#include "ble_client_state.h"
#include "ble_device.h"

#include <concepts>
#include <cstdint>

namespace esphome::ble_device_base {

// Materialized GATT database of a connected peer, discovered by the backend
// and streamed to the API by the consumer. Flat arrays with index ranges
// (not pointers): a service owns characteristics
// [first_characteristic, first_characteristic + characteristic_count) and a
// characteristic owns descriptors [first_descriptor, ...) — discovery is
// depth-first, so the ranges are naturally contiguous.
struct GattDescriptor {
  ESPBTUUID uuid;
  uint16_t handle;
};

struct GattCharacteristic {
  ESPBTUUID uuid;
  uint16_t value_handle;
  // Needed to rebuild the stack's characteristic object for CCCD operations.
  uint16_t end_handle;
  uint8_t properties;  // Bluetooth spec property bitfield
  uint16_t first_descriptor;
  uint16_t descriptor_count;
};

struct GattService {
  ESPBTUUID uuid;
  uint16_t start_handle;
  uint16_t end_handle;
  uint16_t first_characteristic;
  uint16_t characteristic_count;
};

/// Borrowed view of the backend-owned service table. Valid from a successful
/// on_service_discovery_done() until release_services(). Characteristics and
/// descriptors are reached through the per-service/per-characteristic index
/// ranges; the array totals let a consumer bounds-check those ranges instead
/// of trusting the backend's discovery bookkeeping blindly.
struct GattServiceTable {
  const GattService *services{nullptr};
  const GattCharacteristic *characteristics{nullptr};
  const GattDescriptor *descriptors{nullptr};
  uint16_t service_count{0};
  uint16_t characteristic_count{0};
  uint16_t descriptor_count{0};
};

/// The event surface a backend delivers completions through - the one place
/// with genuine runtime polymorphism (several consumer types, one non-virtual
/// backend). Methods default to no-ops; consumers override what they consume.
/// No destructor: components are never destroyed.
/// on_connection_state carries the negotiated MTU and an HCI status/reason.
/// Codegen wires the listener before setup(), so backends skip null checks.
class GattClientListener {
 public:
  virtual void on_connection_state(bool connected, uint16_t mtu, int error) {}
  virtual void on_service_discovery_done(int error) {}
  virtual void on_read_result(uint16_t handle, const uint8_t *data, uint16_t len, int error) {}
  virtual void on_write_result(uint16_t handle, int error) {}
  virtual void on_notify_state(uint16_t handle, bool enabled, int error) {}
  virtual void on_notify_data(uint16_t handle, const uint8_t *data, uint16_t len) {}
  virtual void on_pairing_result(int status) {}
};

// The BLEGattConnection op surface, asserted where the alias binds
// (bluetooth_connection_gatt_backend.h). Operations return 0 when accepted (completion arrives
// through the listener) or a synchronous error (busy, not connected, stack
// rejection); one operation may be outstanding at a time. Semantics beyond
// the signatures:
// - connect: addr_type is a BLE_ADDR_TYPE_* constant (ble_device.h).
// - gatt_disconnect: also cancels a connect in progress (named to coexist
//   with a platform stack's own void disconnect() on one backend class).
//   Nonzero means nothing to tear down and no completion will follow; an
//   accepted teardown (0) always reaches a terminal on_connection_state.
// - cancel_gatt_disconnect: true cancels a scheduled teardown that has not
//   started closing - the in-flight connect resumes and completes normally.
//   False once the teardown owns the link (or nothing was scheduled).
// - notify_characteristic: local registration only; the CCCD write is the
//   API client's responsibility (a plain write_descriptor).
// - get_service_table/release_services: backend-owned transient storage,
//   released after streaming (release is idempotent). A backend may
//   additionally provide its own service streamer (stream_service_batch on
//   the concrete type, detected by the consumer at compile time) for
//   arbitrary-size databases; the table then materializes only for consumers
//   that ask for it.
// - completions: connect and gatt_disconnect land in on_connection_state,
//   discover_services in on_service_discovery_done, pair in
//   on_pairing_result, reads in on_read_result, notify_characteristic in
//   on_notify_state, characteristic writes (with and without response) and
//   descriptor writes in on_write_result.
template<typename T>
concept BLEGattConnectionContract = requires(T conn, GattClientListener *listener, const uint8_t *data) {
  conn.set_listener(listener);
  { conn.connect(uint64_t{}, uint8_t{}) } -> std::same_as<int>;
  { conn.gatt_disconnect() } -> std::same_as<int>;
  { conn.cancel_gatt_disconnect() } -> std::same_as<bool>;
  { conn.discover_services() } -> std::same_as<int>;
  { conn.read_characteristic(uint16_t{}) } -> std::same_as<int>;
  { conn.write_characteristic(uint16_t{}, data, uint16_t{}, true) } -> std::same_as<int>;
  { conn.read_descriptor(uint16_t{}) } -> std::same_as<int>;
  { conn.write_descriptor(uint16_t{}, data, uint16_t{}) } -> std::same_as<int>;
  { conn.notify_characteristic(uint16_t{}, true) } -> std::same_as<int>;
  { conn.pair() } -> std::same_as<int>;
  { conn.update_connection_params(uint16_t{}, uint16_t{}, uint16_t{}, uint16_t{}) } -> std::same_as<int>;
  { conn.get_service_table() } -> std::same_as<GattServiceTable>;
  { conn.release_services() } -> std::same_as<void>;
  // Connection-type hint for backends that tune parameters by it; others
  // carry an inline no-op.
  { conn.set_connection_type(ConnectionType{}) } -> std::same_as<void>;
};

}  // namespace esphome::ble_device_base

#endif  // USE_BLE_GATT_CLIENT
