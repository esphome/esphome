// ble_hub_impl.h
//
// Binds ble_device_base::BLEHub to the build's one tracker; each tracker's
// codegen emits its USE_*_BLE_TRACKER define. Consumers include this header,
// trackers include ble_hub.h (the contract).

#pragma once

#include "ble_hub.h"
#include "esphome/core/defines.h"

#if defined(USE_ESP32_BLE_TRACKER)
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#define ESPHOME_BLE_HUB_TYPE esp32_ble_tracker::ESP32BLETracker
#elif defined(USE_RP2_BLE_TRACKER)
#include "esphome/components/rp2_ble_tracker/rp2_ble_tracker.h"
#define ESPHOME_BLE_HUB_TYPE rp2_ble_tracker::RP2BLETracker
#elif defined(USE_BK72XX_BLE_TRACKER)
#include "esphome/components/bk72xx_ble_tracker/bk72xx_ble_tracker.h"
#define ESPHOME_BLE_HUB_TYPE bk72xx_ble_tracker::BK72xxBLETracker
#elif defined(USE_LN882H_BLE_TRACKER)
#include "esphome/components/ln882h_ble_tracker/ln882h_ble_tracker.h"
#define ESPHOME_BLE_HUB_TYPE ln882h_ble_tracker::LN882HBLETracker
#endif
// No #else on purpose: builds without a tracker (host unit tests) get no BLEHub.

namespace esphome::ble_device_base {

#ifdef ESPHOME_BLE_HUB_TYPE
using BLEHub = ESPHOME_BLE_HUB_TYPE;
static_assert(BLEHubContract<BLEHub>, "The build's BLE tracker is missing part of the BLEHub surface (ble_hub.h)");
#undef ESPHOME_BLE_HUB_TYPE
#endif

}  // namespace esphome::ble_device_base
