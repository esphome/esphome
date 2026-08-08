// ble_gatt_client.h
//
// Platform-neutral GATT client connection contract.
//
// Exactly one GATT backend exists per build, so BLEGattConnection is a
// compile-time alias (bluetooth_connection_gatt_backend.h), not an abstract
// interface.
// The hub BluetoothConnection wrapper drives it and receives completions
// through its event-sink methods, which the backend calls directly. All sink
// calls are delivered on the ESPHome main loop; borrowed data pointers are
// valid only for the duration of the call.
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

// The BLEGattConnection op surface, asserted where the alias binds
// (bluetooth_connection_gatt_backend.h). Operations return 0 when accepted (completion arrives
// through the sink) or a synchronous error (busy, not connected, stack
// rejection); one operation may be outstanding at a time. Semantics beyond
// the signatures:
// - connect: addr_type is a BLE_ADDR_TYPE_* constant (ble_device.h).
// - disconnect: also cancels a connect in progress.
// - notify_characteristic: local registration only; the CCCD write is the
//   API client's responsibility (a plain write_descriptor).
// - get_service_table/release_services: backend-owned transient storage,
//   released after streaming (release is idempotent).
// - completions: connect and disconnect land in on_connection_state,
//   discover_services in on_service_discovery_done, pair in
//   on_pairing_result, reads in on_read_result, notify_characteristic in
//   on_notify_state, characteristic writes with response and descriptor
//   writes in on_write_result.
template<typename T, typename Sink>
concept BLEGattConnectionContract = requires(T conn, Sink *sink, const uint8_t *data) {
  conn.set_listener(sink);
  { conn.connect(uint64_t{}, uint8_t{}) } -> std::same_as<int>;
  { conn.disconnect() } -> std::same_as<int>;
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
};

// The event sink the backend calls directly (the hub BluetoothConnection
// wrapper), asserted where the wrapper is defined: on_connection_state
// carries the negotiated MTU and an HCI status/disconnect reason. The
// requirements check call validity, not exact parameter types; keep sink
// parameters at the documented widths (uint16_t handles and lengths).
template<typename S>
concept GattClientEventSinkContract = requires(S sink, const uint8_t *data) {
  { sink.on_connection_state(true, uint16_t{}, int{}) } -> std::same_as<void>;
  { sink.on_service_discovery_done(int{}) } -> std::same_as<void>;
  { sink.on_read_result(uint16_t{}, data, uint16_t{}, int{}) } -> std::same_as<void>;
  { sink.on_write_result(uint16_t{}, int{}) } -> std::same_as<void>;
  { sink.on_notify_state(uint16_t{}, true, int{}) } -> std::same_as<void>;
  { sink.on_notify_data(uint16_t{}, data, uint16_t{}) } -> std::same_as<void>;
  { sink.on_pairing_result(int{}) } -> std::same_as<void>;
};

}  // namespace esphome::ble_device_base

#endif  // USE_BLE_GATT_CLIENT
