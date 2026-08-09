// ble_gatt_client.h
//
// Platform-neutral GATT client connection contract.
//
// Exactly one GATT backend exists per build, so BLEGattConnection is a
// compile-time alias (bluetooth_connection_gatt_backend.h), not an abstract
// interface.
// A consumer - a streaming consumer that forwards the raw database (the hub
// BluetoothConnection wrapper) or a direct consumer owning a dedicated
// backend and resolving handles by UUID - drives it and receives
// completions through a GattEventSink — a pointer-sized-entry function table
// rather than a concrete consumer type, because one build can hold several
// consumer types while the backend stays a single non-virtual class. All sink
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

// The event sink the backend calls directly, asserted where each consumer is
// defined: on_connection_state carries the negotiated MTU and an HCI
// status/disconnect reason. The requirements check call validity, not exact
// parameter types; keep sink parameters at the documented widths (uint16_t
// handles and lengths).
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

/// One trampoline per event, shared by every instance of a consumer type.
struct GattEventVTable {
  void (*connection_state)(void *, bool, uint16_t, int);
  void (*service_discovery_done)(void *, int);
  void (*read_result)(void *, uint16_t, const uint8_t *, uint16_t, int);
  void (*write_result)(void *, uint16_t, int);
  void (*notify_state)(void *, uint16_t, bool, int);
  void (*notify_data)(void *, uint16_t, const uint8_t *, uint16_t);
  void (*pairing_result)(void *, int);
};

// The per-consumer-type table lives in flash (constexpr), so a sink costs
// two pointers of RAM regardless of how many events the surface carries.
template<typename T>
inline constexpr GattEventVTable GATT_EVENT_VTABLE{
    [](void *p, bool connected, uint16_t mtu, int error) {
      static_cast<T *>(p)->on_connection_state(connected, mtu, error);
    },
    [](void *p, int error) { static_cast<T *>(p)->on_service_discovery_done(error); },
    [](void *p, uint16_t handle, const uint8_t *data, uint16_t len, int error) {
      static_cast<T *>(p)->on_read_result(handle, data, len, error);
    },
    [](void *p, uint16_t handle, int error) { static_cast<T *>(p)->on_write_result(handle, error); },
    [](void *p, uint16_t handle, bool enabled, int error) {
      static_cast<T *>(p)->on_notify_state(handle, enabled, error);
    },
    [](void *p, uint16_t handle, const uint8_t *data, uint16_t len) {
      static_cast<T *>(p)->on_notify_data(handle, data, len);
    },
    [](void *p, int status) { static_cast<T *>(p)->on_pairing_result(status); },
};

/// Type-erased consumer handle a backend delivers events through: an
/// instance pointer plus the consumer type's trampoline table. Built with
/// make_gatt_sink() from any type satisfying GattClientEventSinkContract;
/// call sites read the same as a direct listener call. No virtuals, no heap —
/// the cost of supporting several consumer types in one build is one
/// indirect call per event. Codegen wires the sink before setup(), so
/// backends may call without a null check.
struct GattEventSink {
  void *instance{nullptr};
  const GattEventVTable *vtable{nullptr};

  void on_connection_state(bool connected, uint16_t mtu, int error) const {
    this->vtable->connection_state(this->instance, connected, mtu, error);
  }
  void on_service_discovery_done(int error) const { this->vtable->service_discovery_done(this->instance, error); }
  void on_read_result(uint16_t handle, const uint8_t *data, uint16_t len, int error) const {
    this->vtable->read_result(this->instance, handle, data, len, error);
  }
  void on_write_result(uint16_t handle, int error) const { this->vtable->write_result(this->instance, handle, error); }
  void on_notify_state(uint16_t handle, bool enabled, int error) const {
    this->vtable->notify_state(this->instance, handle, enabled, error);
  }
  void on_notify_data(uint16_t handle, const uint8_t *data, uint16_t len) const {
    this->vtable->notify_data(this->instance, handle, data, len);
  }
  void on_pairing_result(int status) const { this->vtable->pairing_result(this->instance, status); }
};

template<typename T> GattEventSink make_gatt_sink(T *consumer) {
  static_assert(GattClientEventSinkContract<T>, "the consumer is missing part of the event-sink surface");
  return {consumer, &GATT_EVENT_VTABLE<T>};
}

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
//   released after streaming (release is idempotent). A backend may
//   additionally provide its own service streamer (stream_service_batch on
//   the concrete type, detected by the consumer at compile time) for
//   arbitrary-size databases; the table then materializes only for consumers
//   that ask for it.
// - completions: connect and disconnect land in on_connection_state,
//   discover_services in on_service_discovery_done, pair in
//   on_pairing_result, reads in on_read_result, notify_characteristic in
//   on_notify_state, characteristic writes with response and descriptor
//   writes in on_write_result.
template<typename T>
concept BLEGattConnectionContract = requires(T conn, GattEventSink sink, const uint8_t *data) {
  conn.set_sink(sink);
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
  // Deferred-disconnect visibility and the connection-type hint; backends
  // without the underlying state carry inline no-ops.
  { conn.disconnect_pending() } -> std::same_as<bool>;
  { conn.cancel_pending_disconnect() } -> std::same_as<void>;
  { conn.set_connection_type(ConnectionType{}) } -> std::same_as<void>;
};

// ---- service table lookup helpers ----
//
// Neutral, bounds-checked walks over a materialized GattServiceTable for
// direct consumers that resolve a known device's handles by UUID (streaming
// consumers forward the raw database and never need these). Linear search:
// the table exists only between discovery and release_services(), for one
// small known device.

/// Client Characteristic Configuration descriptor UUID (Bluetooth spec).
static constexpr uint16_t CCCD_UUID = 0x2902;

inline const GattService *find_service(const GattServiceTable &table, const ESPBTUUID &uuid) {
  for (uint16_t i = 0; i < table.service_count; i++) {
    if (table.services[i].uuid == uuid)
      return &table.services[i];
  }
  return nullptr;
}

inline const GattCharacteristic *find_characteristic(const GattServiceTable &table, const GattService &service,
                                                     const ESPBTUUID &uuid) {
  uint16_t end = service.first_characteristic + service.characteristic_count;
  if (end > table.characteristic_count)
    return nullptr;
  for (uint16_t i = service.first_characteristic; i < end; i++) {
    if (table.characteristics[i].uuid == uuid)
      return &table.characteristics[i];
  }
  return nullptr;
}

/// Handle of the characteristic's Client Characteristic Configuration
/// descriptor (0x2902), or 0 when it has none.
inline uint16_t find_cccd(const GattServiceTable &table, const GattCharacteristic &characteristic) {
  uint16_t end = characteristic.first_descriptor + characteristic.descriptor_count;
  if (end > table.descriptor_count)
    return 0;
  const ESPBTUUID cccd_uuid = ESPBTUUID::from_uint16(CCCD_UUID);
  for (uint16_t i = characteristic.first_descriptor; i < end; i++) {
    if (table.descriptors[i].uuid == cccd_uuid)
      return table.descriptors[i].handle;
  }
  return 0;
}

}  // namespace esphome::ble_device_base

#endif  // USE_BLE_GATT_CLIENT
