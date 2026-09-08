#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BK72XX_BLE

#include <cstdint>

namespace esphome::bk72xx_ble {

/// Activity index value marking "no scan activity", the BDK's own convention
/// (asserted against its symbol in bdk_scan.cpp).
inline constexpr uint8_t INVALID_ACTIVITY_IDX = 0xFF;

/// Scan-relevant controller activity states, read live from the SDK.
enum class BdkActivityState : uint8_t {
  IDLE,     ///< No activity (or one whose create failed).
  CREATED,  ///< Created but not started.
  STARTED,  ///< Scanning.
  OTHER,    ///< A non-scan or transitional state; settles on a later read.
};

/// Outcome of a BDK scan operation request.
enum class BdkOpResult : uint8_t {
  OK,      ///< Accepted; completion is asynchronous.
  BUSY,    ///< Another controller operation is in flight; retry later.
  FAILED,  ///< Rejected.
};

/// True when no controller operation is in flight (APP_BLE_READY).
bool bdk_scan_ready();
/// Live state of the given activity; INVALID_ACTIVITY_IDX reads as IDLE.
BdkActivityState bdk_scan_state(uint8_t activity_idx);
/// Claim an idle activity slot; INVALID_ACTIVITY_IDX when none is free.
uint8_t bdk_scan_acquire_activity();
/// Create the scan activity (asynchronous); started once CREATED is observed.
BdkOpResult bdk_scan_create(uint8_t activity_idx);
/// Start a created activity: the packed GAPM start, taking the scan mode the
/// BDK's own start path hardcodes away. Fire-and-forget; FAILED when the
/// kernel message could not be allocated (the armed SDK operation is rolled
/// back).
BdkOpResult bdk_scan_start(uint8_t activity_idx, uint16_t interval, uint16_t window, bool active);
/// Release the activity: delete when never started (a stop would be
/// rejected), stop otherwise. BUSY on a transient rejection (retry), FAILED
/// on any other error; err_out receives the SDK code (0 on success).
/// Teardown is asynchronous — observe IDLE to confirm.
BdkOpResult bdk_scan_release(uint8_t activity_idx, bool created, int *err_out);

}  // namespace esphome::bk72xx_ble

#endif  // USE_BK72XX_BLE
