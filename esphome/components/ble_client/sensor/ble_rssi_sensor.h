#pragma once

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#ifdef USE_ESP32
#include <esp_gattc_api.h>

namespace esphome::ble_client {

namespace espbt = esphome::esp32_ble_tracker;

class BLEClientRSSISensor : public sensor::Sensor, public PollingComponent, public BLEClientNode {
 public:
  void loop() override;
  void update() override;
  void dump_config() override;

  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

 protected:
  void get_rssi_();
  bool should_update_{false};
};

}  // namespace esphome::ble_client
#endif
