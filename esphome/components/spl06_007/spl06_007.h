#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace spl06_007 {

/// Internal struct storing the calibration values of an SPL06-007.
struct SPL06_007CalibrationData {
  // temperature calibration coefficients
  int16_t c0;
  int16_t c1;

  // pressure calibration coefficients
  int32_t c00;
  int32_t c10;
  int16_t c01;
  int16_t c11;
  int16_t c20;
  int16_t c21;
  int16_t c30;
};

/** Enum listing all Temperature Oversampling values for the SPL06-007.
 *
 * Oversampling basically means measuring a condition multiple times. Higher oversampling
 * values therefore increase the time required to read sensor values but increase accuracy.
 */
enum SPL06_007TemperatureOversampling {
  SPL06_007_TEMPERATURE_OVERSAMPLING_1X = 0b000,  // default
  SPL06_007_TEMPERATURE_OVERSAMPLING_2X = 0b001,
  SPL06_007_TEMPERATURE_OVERSAMPLING_4X = 0b010,
  SPL06_007_TEMPERATURE_OVERSAMPLING_8X = 0b011,
  SPL06_007_TEMPERATURE_OVERSAMPLING_16X = 0b100,
  SPL06_007_TEMPERATURE_OVERSAMPLING_32X = 0b101,
  SPL06_007_TEMPERATURE_OVERSAMPLING_64X = 0b110,
  SPL06_007_TEMPERATURE_OVERSAMPLING_128X = 0b111,
};

/** Enum listing all Pressure Oversampling values for the SPL06-007.
 *
 * Oversampling basically means measuring a condition multiple times. Higher oversampling
 * values therefore increase the time required to read sensor values but increase accuracy.
 * Note oversampling above 8 requires use of interrupts and are not yet supported
 */

enum SPL06_007PressureOversampling {
  SPL06_007_PRESSURE_OVERSAMPLING_1X = 0b0000,
  SPL06_007_PRESSURE_OVERSAMPLING_2X = 0b0001,  // Low Power.
  SPL06_007_PRESSURE_OVERSAMPLING_4X = 0b0010,
  SPL06_007_PRESSURE_OVERSAMPLING_8X = 0b0011,
  SPL06_007_PRESSURE_OVERSAMPLING_16X = 0b0100,  // Standard.
  SPL06_007_PRESSURE_OVERSAMPLING_32X = 0b0101,
  SPL06_007_PRESSURE_OVERSAMPLING_64X = 0b0110,  // High Precision
  SPL06_007_PRESSURE_OVERSAMPLING_128X = 0b0111
};

/// This class implements support for the SPL06-007 Temperature+Pressure i2c sensor.
class SPL06_007Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { pressure_sensor_ = pressure_sensor; }

  /// Set the oversampling value for the temperature sensor. Default is 2x.
  void set_temperature_oversampling(SPL06_007TemperatureOversampling temperature_over_sampling);
  /// Set the oversampling value for the pressure sensor. Default is 2x.
  void set_pressure_oversampling(SPL06_007PressureOversampling pressure_over_sampling);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

 protected:
  float read_temperature_(const uint8_t *data);
  float read_pressure_(const uint8_t *data);
  uint8_t read_u8_(uint8_t a_register);

  SPL06_007CalibrationData calibration_;
  SPL06_007TemperatureOversampling temperature_oversampling_{SPL06_007_TEMPERATURE_OVERSAMPLING_16X};
  SPL06_007PressureOversampling pressure_oversampling_{SPL06_007_PRESSURE_OVERSAMPLING_16X};
  double_t temp_compensation_factor_{0};
  double_t pressure_compensation_factor_{0};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
    WRONG_CHIP_ID,
  } error_code_{NONE};
};

}  // namespace spl06_007
}  // namespace esphome
