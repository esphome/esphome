#pragma once
#include "esphome/core/defines.h"
#include "ble.h"

#ifdef USE_ESP32
#include "esphome/components/esp32_ble/ble.h"
#ifndef CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID
#include <esp_bt.h>
#endif
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include "esp_bt_device.h"

// ---------------------------------------------------------------------------
// ESP32BLEAdapter — maps to ESP-IDF BLE calls
// ---------------------------------------------------------------------------
namespace esphome {
namespace bthome {

#ifdef USE_BTHOME_SERVER

class ESP32BLEAdapter : public IBLEAdapter, public esp32_ble::GAPEventHandler {
 private:
  bool advertising_{false};

 public:
  MacAddressPtr get_local_mac() override;

  void setup(IBLEAdvHandler *adv_handler) override;

  void config_adv_data_raw(const uint8_t *data, size_t len) override;

  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
};

#endif  // USE_BTHOME_SERVER

}  // namespace bthome
}  // namespace esphome

#endif  // USE_ESP32
