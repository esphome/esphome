#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace bme680 {

/// Enum listing all IIR Filter options for the BME680.
enum BME680IIRFilter {
  BME680_IIR_FILTER_OFF = 0b000,
  BME680_IIR_FILTER_1X = 0b001,
  BME680_IIR_FILTER_3X = 0b010,
  BME680_IIR_FILTER_7X = 0b011,
  BME680_IIR_FILTER_15X = 0b100,
  BME680_IIR_FILTER_31X = 0b101,
  BME680_IIR_FILTER_63X = 0b110,
  BME680_IIR_FILTER_127X = 0b111,
};

/// Enum listing all oversampling options for the BME680.
enum BME680Oversampling {
  BME680_OVERSAMPLING_NONE = 0b000,
  BME680_OVERSAMPLING_1X = 0b001,
  BME680_OVERSAMPLING_2X = 0b010,
  BME680_OVERSAMPLING_4X = 0b011,
  BME680_OVERSAMPLING_8X = 0b100,
  BME680_OVERSAMPLING_16X = 0b101,
};

/// Struct for storing calibration data for the BME680.
struct BME680CalibrationData {
  uint16_t t1;
  uint16_t t2;
  uint8_t t3;

  uint16_t p1;
  int16_t p2;
  int8_t p3;
  int16_t p4;
  int16_t p5;
  int8_t p6;
  int8_t p7;
  int16_t p8;
  int16_t p9;
  int8_t p10;

  uint16_t h1;
  uint16_t h2;
  int8_t h3;
  int8_t h4;
  int8_t h5;
  uint8_t h6;
  int8_t h7;

  int8_t gh1;
  int16_t gh2;
  int8_t gh3;

  uint8_t res_heat_range;
  uint8_t res_heat_val;
  uint8_t range_sw_err;

  float tfine;
  uint8_t ambient_temperature;
};

class BME680Component : public PollingComponent, public i2c::I2CDevice {
 public:
  /// Set the temperature oversampling value. Defaults to 16X.
  void set_temperature_oversampling(BME680Oversampling temperature_oversampling);
  /// Set the pressure oversampling value. Defaults to 16X.
  void set_pressure_oversampling(BME680Oversampling pressure_oversampling);
  /// Set the humidity oversampling value. Defaults to 16X.
  void set_humidity_oversampling(BME680Oversampling humidity_oversampling);
  /// Set the IIR Filter value. Defaults to no IIR Filter.
  void set_iir_filter(BME680IIRFilter iir_filter);

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { pressure_sensor_ = pressure_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }
  void set_gas_resistance_sensor(sensor::Sensor *gas_resistance_sensor) {
    gas_resistance_sensor_ = gas_resistance_sensor;
  }

  /** Set how the internal heater should operate.
   *
   * By default, the heater is off. If you want to to have more reliable
   * humidity and Gas Resistance values, you can however setup the heater
   * with this method.
   *
   * @param heater_temperature The temperature of the heater in °C.
   * @param heater_duration The duration in ms that the heater should turn on for when measuring.
   */
  void set_heater(uint16_t heater_temperature, uint16_t heater_duration);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

 protected:
  /// Calculate the heater resistance value to send to the BME680 register.
  uint8_t calc_heater_resistance_(uint16_t temperature);
  /// Calculate the heater duration value to send to the BME680 register.
  uint8_t calc_heater_duration_(uint16_t duration);
  /// Read data from the BME680 and publish results.
  void read_data_();

  /// Calculate the temperature in °C using the provided raw ADC value.
  float calc_temperature_(uint32_t raw_temperature);
  /// Calculate the pressure in hPa using the provided raw ADC value.
  float calc_pressure_(uint32_t raw_pressure);
  /// Calculate the relative humidity in % using the provided raw ADC value.
  float calc_humidity_(uint16_t raw_humidity);
  /// Calculate the gas resistance in Ω using the provided raw ADC value.
  uint32_t calc_gas_resistance_(uint16_t raw_gas, uint8_t range);
  /// Calculate how long the sensor will take until we can retrieve data.
  uint32_t calc_meas_duration_();

  BME680CalibrationData calibration_;
  BME680Oversampling temperature_oversampling_{BME680_OVERSAMPLING_16X};
  BME680Oversampling pressure_oversampling_{BME680_OVERSAMPLING_16X};
  BME680Oversampling humidity_oversampling_{BME680_OVERSAMPLING_16X};
  BME680IIRFilter iir_filter_{BME680_IIR_FILTER_OFF};
  uint16_t heater_temperature_{320};
  uint16_t heater_duration_{150};

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *gas_resistance_sensor_{nullptr};
};

}  // namespace bme680
}  // namespace esphome
