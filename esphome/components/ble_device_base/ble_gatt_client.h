// ble_gatt_client.h
//
// Platform-neutral GATT client connection contract.
//
// A platform's GATT client backend (bluetooth_connection/esp32,
// bluetooth_connection/rp2) implements BLEGattConnection; consumers
// (bluetooth_proxy) drive it through this interface and receive
// completions through GattClientEventListener. All listener callbacks are
// delivered on the ESPHome main loop; borrowed data pointers are valid only
// for the duration of the call.
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

/// Completion/event sink for a GATT connection. Implemented by the consumer
/// (bluetooth_proxy's connection wrapper). Every callback runs on the main loop.
class GattClientEventListener {
 public:
  virtual ~GattClientEventListener() = default;

  /// Connected (with negotiated MTU) or disconnected/connect-failed
  /// (error = HCI status or disconnect reason).
  virtual void on_connection_state(bool connected, uint16_t mtu, int error) = 0;
  /// Service discovery finished; on success the service table is populated.
  virtual void on_service_discovery_done(int error) = 0;
  /// Characteristic or descriptor read finished. data/len valid during the call.
  virtual void on_read_result(uint16_t handle, const uint8_t *data, uint16_t len, int error) = 0;
  /// Characteristic write-with-response or descriptor write finished.
  virtual void on_write_result(uint16_t handle, int error) = 0;
  /// Notification/indication registration state changed.
  virtual void on_notify_state(uint16_t handle, bool enabled, int error) = 0;
  /// Notification/indication data from the peer. data/len valid during the call.
  virtual void on_notify_data(uint16_t handle, const uint8_t *data, uint16_t len) = 0;
};

/// One GATT client connection slot. Operations return 0 when accepted
/// (completion arrives via the listener) or a synchronous error code
/// (busy, not connected, stack rejection). One operation may be outstanding
/// at a time; callers see a synchronous error otherwise.
class BLEGattConnection {
 public:
  virtual ~BLEGattConnection() = default;

  void set_listener(GattClientEventListener *listener) { this->listener_ = listener; }

  /// Start connecting to a peer. addr_type is a BLE_ADDR_TYPE_* constant
  /// (ble_device.h). Completion: on_connection_state().
  virtual int connect(uint64_t address, uint8_t addr_type) = 0;
  /// Disconnect (or cancel a connect in progress). Completion: on_connection_state().
  virtual int disconnect() = 0;
  /// Discover the peer's services/characteristics/descriptors into the
  /// service table. Completion: on_service_discovery_done().
  virtual int discover_services() = 0;
  virtual int read_characteristic(uint16_t handle) = 0;
  virtual int write_characteristic(uint16_t handle, const uint8_t *data, uint16_t len, bool response) = 0;
  virtual int read_descriptor(uint16_t handle) = 0;
  virtual int write_descriptor(uint16_t handle, const uint8_t *data, uint16_t len) = 0;
  /// Enable/disable delivery of on_notify_data() for a characteristic value
  /// handle. Local registration only — the CCCD write is the API client's
  /// responsibility (it arrives as a plain write_descriptor).
  virtual int notify_characteristic(uint16_t handle, bool enable) = 0;
  virtual int update_connection_params(uint16_t min_interval, uint16_t max_interval, uint16_t latency,
                                       uint16_t timeout) = 0;

  /// Backend-owned service table (see GattServiceTable lifetime).
  virtual GattServiceTable get_service_table() = 0;
  /// Free the transient service table storage. Call after streaming.
  virtual void release_services() = 0;

 protected:
  GattClientEventListener *listener_{nullptr};
};

}  // namespace esphome::ble_device_base

#endif  // USE_BLE_GATT_CLIENT
