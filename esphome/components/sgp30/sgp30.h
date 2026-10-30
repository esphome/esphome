#pragma once

#include "esphome/components/sensirion_common/i2c_sensirion.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

#include <cinttypes>
#include <cmath>

namespace esphome::sgp30 {

struct SGP30Baselines {
  uint16_t eco2;
  uint16_t tvoc;
} PACKED;

/// Absolute humidity in g/m³ from temperature in °C and relative humidity in %, for the compensation command.
/// Buck (1981) over ice below 0 °C, Buck (1996) over water above: https://en.wikipedia.org/wiki/Arden_Buck_equation
inline float compensation_absolute_humidity(float temperature, float humidity) {
  float a, b, c, d;
  if (temperature < 0) {
    a = 0.061115f;
    b = 23.036f;
    c = 333.7f;
    d = 279.82f;
  } else {
    a = 0.061121f;
    b = 18.678f;
    c = 234.5f;
    d = 257.14f;
  }
  return 216.67f * ((humidity * a * std::exp((b - temperature / c) * (temperature / (d + temperature)))) /
                    (273.15f + temperature));
}

/// This class implements support for the Sensirion SGP30 i2c GAS (VOC and CO2eq) sensors.
class SGP30Component final : public PollingComponent, public sensirion_common::SensirionI2CDevice {
  SUB_SENSOR(eco2)
  SUB_SENSOR(tvoc)
  SUB_SENSOR(eco2_baseline)
  SUB_SENSOR(tvoc_baseline)
  // Input sensors for humidity and temperature compensation
  SUB_SENSOR(humidity)
  SUB_SENSOR(temperature)

 public:
  void set_store_baseline(bool store_baseline) { store_baseline_ = store_baseline; }
  void set_eco2_baseline(uint16_t eco2_baseline) { eco2_baseline_ = eco2_baseline; }
  void set_tvoc_baseline(uint16_t tvoc_baseline) { tvoc_baseline_ = tvoc_baseline; }

  void setup() override;
  void update() override;
  void dump_config() override;

 protected:
  void send_env_data_();
  void read_iaq_baseline_();
  bool is_sensor_baseline_reliable_();
  void write_iaq_baseline_(uint16_t eco2_baseline, uint16_t tvoc_baseline);

  uint64_t serial_number_;
  uint32_t required_warm_up_time_;
  uint32_t seconds_since_last_store_;
  uint16_t featureset_;
  uint16_t eco2_baseline_{0x0000};
  uint16_t tvoc_baseline_{0x0000};
  bool store_baseline_;

  ESPPreferenceObject pref_;
  SGP30Baselines baselines_storage_;
};

}  // namespace esphome::sgp30
