// bluetooth_connection_gatt_backend.h
//
// Binds ble_device_base::BLEGattConnection to the build's one GATT backend.
// Backend and consumer both live in this component, so the ladder does too;
// backends implement ble_gatt_client.h (the neutral contract).

#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BLE_GATT_CLIENT

#include "esphome/components/ble_device_base/ble_gatt_client.h"

#if defined(USE_RP2040_BLE)
#include "bluetooth_connection_rp2.h"
#define ESPHOME_BLE_GATT_CONNECTION_TYPE bluetooth_connection::RP2GattClient
#elif defined(USE_ESP32_BLE)
#include "bluetooth_connection_bluedroid.h"
#define ESPHOME_BLE_GATT_CONNECTION_TYPE bluetooth_connection::BluedroidGattClient
#elif defined(USE_BLE_GATT_CLIENT_STUB_BACKEND)
// Emitted only by the host unit-test manifest: the tests compile the hub
// wrapper standalone, so bind a do-nothing backend. Every other backend-less
// build hits the #error below.
namespace esphome::bluetooth_connection {

class StubGattBackend {
 public:
  void set_listener(ble_device_base::GattClientListener *listener) {}
  int connect(uint64_t address, uint8_t addr_type) { return ble_device_base::GATT_ERR_NOT_CONNECTED; }
  int gatt_disconnect() { return ble_device_base::GATT_ERR_NOT_CONNECTED; }
  bool cancel_gatt_disconnect() { return false; }
  int discover_services() { return ble_device_base::GATT_ERR_NOT_CONNECTED; }
  int read_characteristic(uint16_t handle) { return ble_device_base::GATT_ERR_NOT_CONNECTED; }
  int write_characteristic(uint16_t handle, const uint8_t *data, uint16_t len, bool response) {
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  }
  int read_descriptor(uint16_t handle) { return ble_device_base::GATT_ERR_NOT_CONNECTED; }
  int write_descriptor(uint16_t handle, const uint8_t *data, uint16_t len) {
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  }
  int notify_characteristic(uint16_t handle, bool enable) { return ble_device_base::GATT_ERR_NOT_CONNECTED; }
  int pair() { return ble_device_base::GATT_ERR_NOT_CONNECTED; }
  int update_connection_params(uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout) {
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  }
  ble_device_base::GattServiceTable get_service_table() { return {}; }
  void set_connection_type(ble_device_base::ConnectionType ct) {}
  void release_services() {}
};

}  // namespace esphome::bluetooth_connection
#define ESPHOME_BLE_GATT_CONNECTION_TYPE bluetooth_connection::StubGattBackend
#else
#error "USE_BLE_GATT_CLIENT is set but this build has no GATT backend; add an alias arm here"
#endif

namespace esphome::ble_device_base {

using BLEGattConnection = ESPHOME_BLE_GATT_CONNECTION_TYPE;
static_assert(BLEGattConnectionContract<BLEGattConnection>,
              "The build's GATT backend is missing part of the BLEGattConnection surface (ble_gatt_client.h)");
#undef ESPHOME_BLE_GATT_CONNECTION_TYPE

}  // namespace esphome::ble_device_base

#endif  // USE_BLE_GATT_CLIENT
