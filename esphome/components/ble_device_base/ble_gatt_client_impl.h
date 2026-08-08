// ble_gatt_client_impl.h
//
// Binds ble_device_base::BLEGattConnection to the build's one GATT backend.
// Consumers include this header; backends include ble_gatt_client.h (the
// contract).

#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BLE_GATT_CLIENT

#include "ble_gatt_client.h"

#if defined(USE_RP2040_BLE)
#include "esphome/components/bluetooth_connection/bluetooth_connection_rp2.h"
#define ESPHOME_BLE_GATT_CONNECTION_TYPE bluetooth_connection::RP2GattClient
namespace esphome::bluetooth_connection {
class BluetoothConnection;
}  // namespace esphome::bluetooth_connection
#else
// No platform engine in this build: host unit tests compile the hub wrapper
// standalone, so bind a do-nothing backend. Real builds cannot land here
// silently; codegen emits USE_BLE_GATT_CLIENT only while wiring a platform
// engine, whose type would then mismatch set_backend().
namespace esphome::bluetooth_connection {
class BluetoothConnection;
}  // namespace esphome::bluetooth_connection

namespace esphome::ble_device_base {
class StubGattBackend {
 public:
  void set_listener(bluetooth_connection::BluetoothConnection *listener) {}
  int connect(uint64_t address, uint8_t addr_type) { return GATT_ERR_NOT_CONNECTED; }
  int disconnect() { return GATT_ERR_NOT_CONNECTED; }
  int discover_services() { return GATT_ERR_NOT_CONNECTED; }
  int read_characteristic(uint16_t handle) { return GATT_ERR_NOT_CONNECTED; }
  int write_characteristic(uint16_t handle, const uint8_t *data, uint16_t len, bool response) {
    return GATT_ERR_NOT_CONNECTED;
  }
  int read_descriptor(uint16_t handle) { return GATT_ERR_NOT_CONNECTED; }
  int write_descriptor(uint16_t handle, const uint8_t *data, uint16_t len) { return GATT_ERR_NOT_CONNECTED; }
  int notify_characteristic(uint16_t handle, bool enable) { return GATT_ERR_NOT_CONNECTED; }
  int pair() { return GATT_ERR_NOT_CONNECTED; }
  int update_connection_params(uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout) {
    return GATT_ERR_NOT_CONNECTED;
  }
  GattServiceTable get_service_table() { return {}; }
  void release_services() {}
};
}  // namespace esphome::ble_device_base
#define ESPHOME_BLE_GATT_CONNECTION_TYPE StubGattBackend
#endif

namespace esphome::ble_device_base {

using BLEGattConnection = ESPHOME_BLE_GATT_CONNECTION_TYPE;
static_assert(BLEGattConnectionContract<BLEGattConnection, bluetooth_connection::BluetoothConnection>,
              "The build's GATT backend is missing part of the BLEGattConnection surface (ble_gatt_client.h)");
#undef ESPHOME_BLE_GATT_CONNECTION_TYPE

}  // namespace esphome::ble_device_base

#endif  // USE_BLE_GATT_CLIENT
