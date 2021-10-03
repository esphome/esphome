#pragma once

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include <algorithm>
#include <iterator>
#include <esp_gattc_api.h>
#include <BLEDevice.h>
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace airthings_wave_mini {

class AirthingsWaveMini : public PollingComponent, public ble_client::BLEClientNode {
 public:
  AirthingsWaveMini();

  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void set_temperature(sensor::Sensor *temperature) { temperature_sensor_ = temperature; }
  void set_humidity(sensor::Sensor *humidity) { humidity_sensor_ = humidity; }
  void set_tvoc(sensor::Sensor *tvoc) { tvoc_sensor_ = tvoc; }

 protected:
  bool is_valid_voc_value_(uint16_t voc);

  void read_sensors_(uint8_t *value, uint16_t value_len);
  void request_read_values_();

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *tvoc_sensor_{nullptr};

  uint16_t handle_;
  esp32_ble_tracker::ESPBTUUID service_uuid_;
  esp32_ble_tracker::ESPBTUUID sensors_data_characteristic_uuid_;

  struct WaveMiniReadings {
    uint8_t version;
    uint8_t humidity;
    uint8_t ambientLight;
    uint8_t unused01;
    uint16_t radon;
    uint16_t radon_lt;
    uint16_t temperature;
    uint16_t pressure;
    uint16_t co2;
    uint16_t voc;
  };
};

}  // namespace airthings_wave_mini
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
