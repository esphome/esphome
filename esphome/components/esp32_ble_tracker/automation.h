#pragma once

#include "esphome/core/automation.h"
#include "esphome/components/ble_device_base/automation.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#ifdef USE_ESP32

namespace esphome::esp32_ble_tracker {

// The advertisement triggers are the shared ble_device_base ones; on_scan_end stays on the esp32 listener so a
// scan-end-only config does not pull in parsed advertisements.
class BLEEndOfScanTrigger final : public Trigger<>, public ESPBTDeviceListener {
 public:
  explicit BLEEndOfScanTrigger(ESP32BLETracker *parent) { parent->register_listener(this); }

#ifdef USE_ESP32_BLE_DEVICE
  bool parse_device(const ESPBTDevice &device) override { return false; }
#endif
  void on_scan_end() override { this->trigger(); }
};

}  // namespace esphome::esp32_ble_tracker

#endif
