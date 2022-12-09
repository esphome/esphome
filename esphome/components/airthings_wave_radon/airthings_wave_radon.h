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
namespace airthings_wave_radon {

static const char *const SERVICE_UUID = "b42e4a8e-ade7-11e4-89d3-123b93f75cba";
static const char *const CHARACTERISTIC_UUID = "b42e4dcc-ade7-11e4-89d3-123b93f75cba";

class AirthingsWaveRadon : public PollingComponent, public ble_client::BLEClientNode {
 public:
  AirthingsWaveRadon();

  void dump_config() override;
  void update() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void set_temperature(sensor::Sensor *temperature) { temperature_sensor_ = temperature; }
  void set_radon(sensor::Sensor *radon) { radon_sensor_ = radon; }
  void set_radon_long_term(sensor::Sensor *radon_long_term) { radon_long_term_sensor_ = radon_long_term; }
  void set_humidity(sensor::Sensor *humidity) { humidity_sensor_ = humidity; }

 protected:
  bool is_valid_radon_value_(uint16_t radon);

  void read_sensors_(uint8_t *value, uint16_t value_len);
  void request_read_values_();

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *radon_sensor_{nullptr};
  sensor::Sensor *radon_long_term_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};

  uint16_t handle_;
  esp32_ble_tracker::ESPBTUUID service_uuid_;
  esp32_ble_tracker::ESPBTUUID sensors_data_characteristic_uuid_;

  struct WaveRadonReadings {
    uint8_t version;
    uint8_t humidity;
    uint8_t ambientLight;
    uint8_t unused01;
    uint16_t radon;
    uint16_t radon_lt;
    uint16_t temperature;
  };
};

}  // namespace airthings_wave_radon
}  // namespace esphome

#endif  // USE_ESP32
