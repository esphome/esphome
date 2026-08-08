// ble_hub_impl.h
//
// Binds ble_device_base::BLEHub to the build's tracker. Exactly one tracker
// component exists per build, so the hub is a compile-time alias rather than
// an abstract interface: every hub call is a direct, inlinable member call
// and trackers carry no vtable for the contract. Consumers include this
// header; trackers include ble_hub.h (the contract types and the documented
// method surface). Each tracker's codegen emits its USE_*_BLE_TRACKER define.

#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32_BLE_TRACKER)
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#elif defined(USE_RP2_BLE_TRACKER)
#include "esphome/components/rp2_ble_tracker/rp2_ble_tracker.h"
#elif defined(USE_BK72XX_BLE_TRACKER)
#include "esphome/components/bk72xx_ble_tracker/bk72xx_ble_tracker.h"
#elif defined(USE_LN882H_BLE_TRACKER)
#include "esphome/components/ln882h_ble_tracker/ln882h_ble_tracker.h"
#endif

namespace esphome::ble_device_base {

#if defined(USE_ESP32_BLE_TRACKER)
using BLEHub = esp32_ble_tracker::ESP32BLETracker;
#elif defined(USE_RP2_BLE_TRACKER)
using BLEHub = rp2_ble_tracker::RP2BLETracker;
#elif defined(USE_BK72XX_BLE_TRACKER)
using BLEHub = bk72xx_ble_tracker::BK72xxBLETracker;
#elif defined(USE_LN882H_BLE_TRACKER)
using BLEHub = ln882h_ble_tracker::LN882HBLETracker;
#endif

}  // namespace esphome::ble_device_base
