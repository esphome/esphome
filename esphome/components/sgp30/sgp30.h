#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/preferences.h"
#include <cmath>

namespace esphome {
namespace sgp30 {

struct SGP30Baselines {
  uint16_t eco2;
  uint16_t tvoc;
} PACKED;

/// This class implements support for the Sensirion SGP30 i2c GAS (VOC and CO2eq) sensors.
class SGP30Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_eco2_sensor(sensor::Sensor *eco2) { eco2_sensor_ = eco2; }
  void set_tvoc_sensor(sensor::Sensor *tvoc) { tvoc_sensor_ = tvoc; }
  void set_eco2_baseline_sensor(sensor::Sensor *eco2_baseline) { eco2_sensor_baseline_ = eco2_baseline; }
  void set_tvoc_baseline_sensor(sensor::Sensor *tvoc_baseline) { tvoc_sensor_baseline_ = tvoc_baseline; }
  void set_store_baseline(bool store_baseline) { store_baseline_ = store_baseline; }
  void set_eco2_baseline(uint16_t eco2_baseline) { eco2_baseline_ = eco2_baseline; }
  void set_tvoc_baseline(uint16_t tvoc_baseline) { tvoc_baseline_ = tvoc_baseline; }
  void set_humidity_sensor(sensor::Sensor *humidity) { humidity_sensor_ = humidity; }
  void set_temperature_sensor(sensor::Sensor *temperature) { temperature_sensor_ = temperature; }

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  bool write_command_(uint16_t command);
  bool read_data_(uint16_t *data, uint8_t len);
  void send_env_data_();
  void read_iaq_baseline_();
  bool is_sensor_baseline_reliable_();
  void write_iaq_baseline_(uint16_t eco2_baseline, uint16_t tvoc_baseline);
  uint8_t sht_crc_(uint8_t data1, uint8_t data2);
  uint64_t serial_number_;
  uint16_t featureset_;
  uint32_t required_warm_up_time_;
  uint32_t seconds_since_last_store_;
  SGP30Baselines baselines_storage_;
  ESPPreferenceObject pref_;

  enum ErrorCode {
    COMMUNICATION_FAILED,
    MEASUREMENT_INIT_FAILED,
    INVALID_ID,
    UNSUPPORTED_ID,
    UNKNOWN
  } error_code_{UNKNOWN};

  sensor::Sensor *eco2_sensor_{nullptr};
  sensor::Sensor *tvoc_sensor_{nullptr};
  sensor::Sensor *eco2_sensor_baseline_{nullptr};
  sensor::Sensor *tvoc_sensor_baseline_{nullptr};
  uint16_t eco2_baseline_{0x0000};
  uint16_t tvoc_baseline_{0x0000};
  bool store_baseline_;

  /// Input sensor for humidity and temperature compensation.
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
};

}  // namespace sgp30
}  // namespace esphome
