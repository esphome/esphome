#pragma once
#include "esphome/core/defines.h"
#include "ble.h"
#include "bthome.h"

#ifdef USE_ESP32
#include "esphome/components/esp32_ble/ble.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#ifndef CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID
#include <esp_bt.h>
#endif
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include "esp_bt_device.h"

namespace esphome::bthome {

// ---------------------------------------------------------------------------
// ESP32BLEListener — receives BLE advertisements and dispatches BTHome data
// ---------------------------------------------------------------------------

class ESP32BLEListener : public IBLEListener, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void setup(IBTHomeListener *listener) override { this->listener_ = listener; }

 protected:
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;

 private:
  IBTHomeListener *listener_{nullptr};
};

}  // namespace esphome::bthome

#endif  // USE_ESP32
