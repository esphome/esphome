#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace bmp280_base {

/// Internal struct storing the calibration values of an BMP280.
struct BMP280CalibrationData {
  uint16_t t1;  // 0x88 - 0x89
  int16_t t2;   // 0x8A - 0x8B
  int16_t t3;   // 0x8C - 0x8D

  uint16_t p1;  // 0x8E - 0x8F
  int16_t p2;   // 0x90 - 0x91
  int16_t p3;   // 0x92 - 0x93
  int16_t p4;   // 0x94 - 0x95
  int16_t p5;   // 0x96 - 0x97
  int16_t p6;   // 0x98 - 0x99
  int16_t p7;   // 0x9A - 0x9B
  int16_t p8;   // 0x9C - 0x9D
  int16_t p9;   // 0x9E - 0x9F
};

/** Enum listing all Oversampling values for the BMP280.
 *
 * Oversampling basically means measuring a condition multiple times. Higher oversampling
 * values therefore increase the time required to read sensor values but increase accuracy.
 */
enum BMP280Oversampling {
  BMP280_OVERSAMPLING_NONE = 0b000,
  BMP280_OVERSAMPLING_1X = 0b001,
  BMP280_OVERSAMPLING_2X = 0b010,
  BMP280_OVERSAMPLING_4X = 0b011,
  BMP280_OVERSAMPLING_8X = 0b100,
  BMP280_OVERSAMPLING_16X = 0b101,
};

/** Enum listing all Infinite Impulse Filter values for the BMP280.
 *
 * Higher values increase accuracy, but decrease response time.
 */
enum BMP280IIRFilter {
  BMP280_IIR_FILTER_OFF = 0b000,
  BMP280_IIR_FILTER_2X = 0b001,
  BMP280_IIR_FILTER_4X = 0b010,
  BMP280_IIR_FILTER_8X = 0b011,
  BMP280_IIR_FILTER_16X = 0b100,
};

/// This class implements support for the BMP280 Temperature+Pressure sensor.
class BMP280Component : public PollingComponent {
 public:
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { pressure_sensor_ = pressure_sensor; }

  /// Set the oversampling value for the temperature sensor. Default is 16x.
  void set_temperature_oversampling(BMP280Oversampling temperature_over_sampling);
  /// Set the oversampling value for the pressure sensor. Default is 16x.
  void set_pressure_oversampling(BMP280Oversampling pressure_over_sampling);
  /// Set the IIR Filter used to increase accuracy, defaults to no IIR Filter.
  void set_iir_filter(BMP280IIRFilter iir_filter);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  virtual bool read_byte(uint8_t a_register, uint8_t *data) = 0;
  virtual bool write_byte(uint8_t a_register, uint8_t data) = 0;
  virtual bool read_bytes(uint8_t a_register, uint8_t *data, size_t len) = 0;
  virtual bool read_byte_16(uint8_t a_register, uint16_t *data) = 0;

 protected:
  /// Read the temperature value and store the calculated ambient temperature in t_fine.
  float read_temperature_(int32_t *t_fine);
  /// Read the pressure value in hPa using the provided t_fine value.
  float read_pressure_(int32_t t_fine);
  uint8_t read_u8_(uint8_t a_register);
  uint16_t read_u16_le_(uint8_t a_register);
  int16_t read_s16_le_(uint8_t a_register);

  BMP280CalibrationData calibration_;
  BMP280Oversampling temperature_oversampling_{BMP280_OVERSAMPLING_16X};
  BMP280Oversampling pressure_oversampling_{BMP280_OVERSAMPLING_16X};
  BMP280IIRFilter iir_filter_{BMP280_IIR_FILTER_OFF};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
    WRONG_CHIP_ID,
  } error_code_{NONE};
};

}  // namespace bmp280_base
}  // namespace esphome
