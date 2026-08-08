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
#endif
// No #else on purpose: host unit tests compile the contract with no backend.

namespace esphome::ble_device_base {

#ifdef ESPHOME_BLE_GATT_CONNECTION_TYPE
using BLEGattConnection = ESPHOME_BLE_GATT_CONNECTION_TYPE;
static_assert(BLEGattConnectionContract<BLEGattConnection>,
              "The build's GATT backend is missing part of the BLEGattConnection surface (ble_gatt_client.h)");
#undef ESPHOME_BLE_GATT_CONNECTION_TYPE
#endif

}  // namespace esphome::ble_device_base

#endif  // USE_BLE_GATT_CLIENT
