#pragma once

#ifdef USE_ESP32

#include "esphome/components/airthings_wave_base/airthings_wave_base.h"

namespace esphome {
namespace airthings_wave_plus {

static const char *const SERVICE_UUID = "b42e1c08-ade7-11e4-89d3-123b93f75cba";
static const char *const CHARACTERISTIC_UUID = "b42e2a68-ade7-11e4-89d3-123b93f75cba";

class AirthingsWavePlus : public airthings_wave_base::AirthingsWaveBase {
 public:
  AirthingsWavePlus();

  void dump_config() override;

  void set_radon(sensor::Sensor *radon) { radon_sensor_ = radon; }
  void set_radon_long_term(sensor::Sensor *radon_long_term) { radon_long_term_sensor_ = radon_long_term; }
  void set_co2(sensor::Sensor *co2) { co2_sensor_ = co2; }

 protected:
  bool is_valid_radon_value_(uint16_t radon);
  bool is_valid_co2_value_(uint16_t co2);

  void read_sensors(uint8_t *value, uint16_t value_len) override;

  sensor::Sensor *radon_sensor_{nullptr};
  sensor::Sensor *radon_long_term_sensor_{nullptr};
  sensor::Sensor *co2_sensor_{nullptr};

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
