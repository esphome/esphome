#pragma once

#ifdef USE_ESP32

#include "esphome/components/airthings_wave_base/airthings_wave_base.h"

namespace esphome {
namespace airthings_wave_radon {

namespace espbt = esphome::esp32_ble_tracker;

static const char *const SERVICE_UUID = "b42e4a8e-ade7-11e4-89d3-123b93f75cba";
static const char *const CHARACTERISTIC_UUID = "b42e4dcc-ade7-11e4-89d3-123b93f75cba";
static const char *const ACCESS_CONTROL_POINT_CHARACTERISTIC_UUID = "b42e4dcc-ade7-11e4-89d3-123b93f75cba";

class AirthingsWaveRadon : public airthings_wave_base::AirthingsWaveBase {
 public:
  AirthingsWaveRadon();

  void dump_config() override;

  void set_radon(sensor::Sensor *radon) { radon_sensor_ = radon; }
  void set_radon_long_term(sensor::Sensor *radon_long_term) { radon_long_term_sensor_ = radon_long_term; }

 protected:
  bool is_valid_radon_value_(uint16_t radon);

  void read_sensors(uint8_t *raw_value, uint16_t value_len) override;

  sensor::Sensor *radon_sensor_{nullptr};
  sensor::Sensor *radon_long_term_sensor_{nullptr};

  struct WaveRadonReadings {
    uint8_t version;
    uint8_t humidity;
    uint8_t ambientLight;
    uint8_t unused01;
    uint16_t radon;
    uint16_t radon_lt;
    uint16_t temperature;
    uint16_t pressure;
  };
};

}  // namespace airthings_wave_radon
}  // namespace esphome

#endif  // USE_ESP32
