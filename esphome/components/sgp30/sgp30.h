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

/// This class implements support for the Sensirion SGP30 i2c GAS (VOC and CO2eq) sensors.
class SGP30Component final : public PollingComponent, public sensirion_common::SensirionI2CDevice {
  SUB_SENSOR(eco2)
  SUB_SENSOR(tvoc)
  SUB_SENSOR(eco2_baseline)
  SUB_SENSOR(tvoc_baseline)

 public:
  void set_store_baseline(bool store_baseline) { store_baseline_ = store_baseline; }
  void set_eco2_baseline(uint16_t eco2_baseline) { eco2_baseline_ = eco2_baseline; }
  void set_tvoc_baseline(uint16_t tvoc_baseline) { tvoc_baseline_ = tvoc_baseline; }
  void set_humidity_sensor(sensor::Sensor *humidity) { humidity_sensor_ = humidity; }
  void set_temperature_sensor(sensor::Sensor *temperature) { temperature_sensor_ = temperature; }

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

  /// Input sensor for humidity and temperature compensation.
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
};

}  // namespace esphome::sgp30
