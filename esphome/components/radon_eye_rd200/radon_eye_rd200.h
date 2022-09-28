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

namespace esphome {
namespace radon_eye_rd200 {

static const char *const SERVICE_UUID = "00001523-0000-1000-8000-00805f9b34fb";
static const char *const WRITE_CHARACTERISTIC_UUID = "00001524-0000-1000-8000-00805f9b34fb";
static const char *const READ_CHARACTERISTIC_UUID = "00001525-0000-1000-8000-00805f9b34fb";

class RadonEyeRD200 : public PollingComponent, public ble_client::BLEClientNode {
 public:
  RadonEyeRD200();

  void dump_config() override;
  void update() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void set_radon(sensor::Sensor *radon) { radon_sensor_ = radon; }
  void set_radon_long_term(sensor::Sensor *radon_long_term) { radon_long_term_sensor_ = radon_long_term; }

 protected:
  bool is_valid_radon_value_(float radon);

  void read_sensors_(uint8_t *value, uint16_t value_len);
  void write_query_message_();
  void request_read_values_();

  sensor::Sensor *radon_sensor_{nullptr};
  sensor::Sensor *radon_long_term_sensor_{nullptr};

  uint16_t read_handle_;
  uint16_t write_handle_;
  esp32_ble_tracker::ESPBTUUID service_uuid_;
  esp32_ble_tracker::ESPBTUUID sensors_write_characteristic_uuid_;
  esp32_ble_tracker::ESPBTUUID sensors_read_characteristic_uuid_;

  union RadonValue {
    char chars[4];
    float number;
  };
};

}  // namespace radon_eye_rd200
}  // namespace esphome

#endif  // USE_ESP32
