#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace xgzp6897d {

/** Enum listing all Oversampling values for the XGZP6897D.
 *
 * Oversampling basically means measuring a condition multiple times. Higher oversampling
 * values therefore increase the time required to read sensor values but increase accuracy.
 */
enum XGZP6897DOversampling {
  XGZP6897D_OVERSAMPLING_256X = 0b100,
  XGZP6897D_OVERSAMPLING_512X = 0b101,
  XGZP6897D_OVERSAMPLING_1024X = 0b000,
  XGZP6897D_OVERSAMPLING_2048X = 0b001,
  XGZP6897D_OVERSAMPLING_4096X = 0b010,
  XGZP6897D_OVERSAMPLING_8192X = 0b011,
  XGZP6897D_OVERSAMPLING_16384X = 0b110,
  XGZP6897D_OVERSAMPLING_32768X = 0b111,
};

/// This class implements support for the XGZP6897D Differential Pressure i2c sensor.
class XGZP6897DComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { pressure_sensor_ = pressure_sensor; }

  /// Set the oversampling value
  void set_oversampling(XGZP6897DOversampling over_sampling) { this->oversampling_ = over_sampling; }

  // Set kvalue, needed to calculated pressure and depends on the pressure range of the sensor
  void set_kvalue(uint16_t kvalue) { kvalue_ = kvalue; }

  void set_continuous_mode(bool continuous_mode) { continuous_mode_ = continuous_mode; }
  void set_sleep_time(uint8_t sleep_time) { sleep_time_ = sleep_time; }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

 protected:
  /// Read the pressure value in hPa.
  float read_pressure_(const uint8_t *data);
  /// Read the temperature value.
  float read_temperature_(const uint8_t *data);

  XGZP6897DOversampling oversampling_{XGZP6897D_OVERSAMPLING_1024X};
  sensor::Sensor *temperature_sensor_;
  sensor::Sensor *pressure_sensor_;
  uint16_t kvalue_{32};
  bool continuous_mode_{false};
  uint8_t sleep_time_{0};
  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
  } error_code_{NONE};
};

}  // namespace xgzp6897d
}  // namespace esphome
