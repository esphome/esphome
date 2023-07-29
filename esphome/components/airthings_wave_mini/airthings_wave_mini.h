#pragma once

#ifdef USE_ESP32

#include "esphome/components/airthings_wave_base/airthings_wave_base.h"

namespace esphome {
namespace airthings_wave_mini {

namespace espbt = esphome::esp32_ble_tracker;

static const char *const SERVICE_UUID = "b42e3882-ade7-11e4-89d3-123b93f75cba";
static const char *const CHARACTERISTIC_UUID = "b42e3b98-ade7-11e4-89d3-123b93f75cba";
static const char *const ACCESS_CONTROL_POINT_CHARACTERISTIC_UUID = "b42e3ef4-ade7-11e4-89d3-123b93f75cba";

class AirthingsWaveMini : public airthings_wave_base::AirthingsWaveBase {
 public:
  AirthingsWaveMini();

  void dump_config() override;

 protected:
  void read_sensors(uint8_t *raw_value, uint16_t value_len) override;

  struct WaveMiniReadings {
    uint16_t unused01;
    uint16_t temperature;
    uint16_t pressure;
    uint16_t humidity;
    uint16_t voc;
    uint16_t unused02;
    uint32_t unused03;
    uint32_t unused04;
  };
};

}  // namespace airthings_wave_mini
}  // namespace esphome

#endif  // USE_ESP32
