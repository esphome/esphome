#pragma once
#include "esphome/core/defines.h"
#include "ble.h"
#include "helpers.h"

#ifdef USE_ESP32
#include "esphome/components/esp32_ble/ble.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#ifndef CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID
#include <esp_bt.h>
#endif
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include "esp_bt_device.h"

namespace esphome {
namespace bthome {

// ---------------------------------------------------------------------------
// ESP32BLEAdvertiser — maps to ESP-IDF BLE advertising calls
// ---------------------------------------------------------------------------

#ifdef USE_BTHOME_SERVER

class ESP32BLEAdvertiser : public IBLEAdvertiser, public esp32_ble::GAPEventHandler {
 private:
  bool advertising_{false};

 public:
  MacAddressPtr get_local_mac() override;

  void setup(IBLEAdvHandler *adv_handler) override;

  void config_adv_data_raw(const uint8_t *data, size_t len) override;

  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
};

#endif  // USE_BTHOME_SERVER

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

}  // namespace bthome
}  // namespace esphome

#endif  // USE_ESP32
