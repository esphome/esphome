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
namespace airthings_wave_plus {

static const char *const SERVICE_UUID = "b42e1c08-ade7-11e4-89d3-123b93f75cba";
static const char *const CHARACTERISTIC_UUID = "b42e2a68-ade7-11e4-89d3-123b93f75cba";

class AirthingsWavePlus : public PollingComponent, public ble_client::BLEClientNode {
 public:
  AirthingsWavePlus();

  void dump_config() override;
  void update() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void set_temperature(sensor::Sensor *temperature) { temperature_sensor_ = temperature; }
  void set_radon(sensor::Sensor *radon) { radon_sensor_ = radon; }
  void set_radon_long_term(sensor::Sensor *radon_long_term) { radon_long_term_sensor_ = radon_long_term; }
  void set_humidity(sensor::Sensor *humidity) { humidity_sensor_ = humidity; }
  void set_pressure(sensor::Sensor *pressure) { pressure_sensor_ = pressure; }
  void set_co2(sensor::Sensor *co2) { co2_sensor_ = co2; }
  void set_tvoc(sensor::Sensor *tvoc) { tvoc_sensor_ = tvoc; }

 protected:
  bool is_valid_radon_value_(uint16_t radon);
  bool is_valid_voc_value_(uint16_t voc);
  bool is_valid_co2_value_(uint16_t co2);

  void read_sensors_(uint8_t *value, uint16_t value_len);
  void request_read_values_();

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *radon_sensor_{nullptr};
  sensor::Sensor *radon_long_term_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *tvoc_sensor_{nullptr};

  uint16_t handle_;
  esp32_ble_tracker::ESPBTUUID service_uuid_;
  esp32_ble_tracker::ESPBTUUID sensors_data_characteristic_uuid_;

  struct WavePlusReadings {
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

}  // namespace airthings_wave_plus
}  // namespace esphome

#endif  // USE_ESP32
