#pragma once

#ifdef USE_ESP32

#include <esp_gattc_api.h>
#include <algorithm>
#include <iterator>
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome::radon_eye_rd200 {

class RadonEyeRD200 final : public PollingComponent, public ble_client::BLEClientNode {
  SUB_SENSOR(radon)
  SUB_SENSOR(radon_long_term)

 public:
  RadonEyeRD200();

  void dump_config() override;
  void update() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

 protected:
  void read_sensors_(uint8_t *value, uint16_t value_len);

  uint8_t write_command_;
  uint16_t read_handle_;
  uint16_t write_handle_;
  esp32_ble_tracker::ESPBTUUID service_uuid_;
  esp32_ble_tracker::ESPBTUUID sensors_write_characteristic_uuid_;
  esp32_ble_tracker::ESPBTUUID sensors_read_characteristic_uuid_;
};

}  // namespace esphome::radon_eye_rd200

#endif  // USE_ESP32
