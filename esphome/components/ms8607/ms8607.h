#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::ms8607 {

/// Pressure & temperature calibration values, read from the PROM
struct CalibrationValues {
  /// Pressure sensitivity | SENS-T1. [C1]
  uint16_t pressure_sensitivity;
  /// Temperature coefficient of pressure sensitivity | TCS. [C3]
  uint16_t pressure_sensitivity_temperature_coefficient;
  /// Pressure offset | OFF-T1. [C2]
  uint16_t pressure_offset;
  /// Temperature coefficient of pressure offset | TCO. [C4]
  uint16_t pressure_offset_temperature_coefficient;
  /// Reference temperature | T-REF. [C5]
  uint16_t reference_temperature;
  /// Temperature coefficient of the temperature | TEMPSENS. [C6]
  uint16_t temperature_coefficient_of_temperature;
};

struct CompensatedTemperature {
  /// difference between actual and reference temperature
  int32_t d_t;
  /// temperature as hundredths of degree celsius in range [-4000, 8500], after first order
  /// temperature calculation
  int32_t first_order_temperature;
  /// actual temperature, after 2nd order temperature calculation, in degrees celsius as a float
  float temperature_float;
};

/// use raw temperature and calibration values to figure out actual temperature value
/// return value includes some intermediate values needed by the pressure compensation algorithm
/// Precondition: values must be within typical values from the device to avoid Undefined Behavior.
CompensatedTemperature compensated_temperature(uint32_t d2_raw_temperature,
                                               const CalibrationValues &calibration_values);

/// use raw pressure, calibration values, and current temperature to figure out actual pressure
/// Precondition: calibration_values & temperature_values must be within typical values for the device.
/// Unbounded values can cause Undefined Behavior.
float compensated_pressure(uint32_t d1_raw_pressure, const CalibrationValues &calibration_values,
                           const CompensatedTemperature &temperature_values);

/// convert raw humidity value into correct range & apply temperature compensation calculation
float compensated_humidity(float humidity_float, float temperature_float);

/// I2C device for the humidity sensor's own address on the chip. See MS8607Component instead
class MS8607HumidityDevice final : public i2c::I2CDevice {};

/**
 Temperature, pressure, and humidity sensor.

 By default, the MS8607 measures sensors at the highest resolution.
 A potential enhancement would be to expose the resolution as a configurable
 setting.  A lower resolution speeds up ADC conversion time & uses less power.

 Datasheet:
 https://www.te.com/commerce/DocumentDelivery/DDEController?Action=showdoc&DocId=Data+Sheet%7FMS8607-02BA01%7FB3%7Fpdf%7FEnglish%7FENG_DS_MS8607-02BA01_B3.pdf%7FCAT-BLPS0018

 Other implementations:
 - https://github.com/TEConnectivity/MS8607_Generic_C_Driver
 - https://github.com/adafruit/Adafruit_MS8607
 - https://github.com/sparkfun/SparkFun_PHT_MS8607_Arduino_Library
 */
class MS8607Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(temperature)
  SUB_SENSOR(pressure)
  SUB_SENSOR(humidity)

 public:
  ~MS8607Component() = default;
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_humidity_device(MS8607HumidityDevice *humidity_device) { this->humidity_device_ = humidity_device; }

 protected:
  /// Attempt to reset both I2C devices, retrying with backoff on failure
  void try_reset_();
  /// Read and CRC-check the PROM calibration values; marks the component failed on error
  bool read_calibration_values_from_prom_();
  /// Read a 24-bit converted ADC value, either D1 (pressure) or D2 (temperature)
  bool read_adc_(uint32_t &value);

  /// Start async temperature read
  void request_read_temperature_();
  /// Process async temperature read, publish it and hand off to pressure or humidity
  void read_temperature_();
  /// Start async pressure read; false if the request failed
  bool request_read_pressure_(uint32_t d2_raw_temperature);
  /// Process async pressure read and publish it. Takes the raw temperature rather than the
  /// compensated one so the timeout callback fits in std::function's inline storage
  void read_pressure_(uint32_t d2_raw_temperature);
  /// Read humidity if it is configured, otherwise end the update cycle
  void finish_update_(float temperature);
  /// start async humidity read
  void request_read_humidity_(float temperature);
  /// process async humidity read
  void read_humidity_(float temperature);
  /// End the update cycle, setting or clearing the warning from update_ok_
  void finalize_update_();

  /** I2CDevice object to communicate with secondary I2C address for the humidity sensor
   *
   * The MS8607 only has one set of I2C pins, despite using two different addresses.
   * Set exactly when the humidity sensor is configured. Default address for humidity is 0x40
   */
  MS8607HumidityDevice *humidity_device_{nullptr};

  /// This device's pressure & temperature calibration values, read from PROM
  CalibrationValues calibration_values_{};

  /// Set once the PROM calibration values have been read
  bool setup_complete_{false};
  uint8_t reset_interval_{5};
  uint8_t reset_attempts_remaining_{3};
  /// False once any step of the current update cycle failed
  bool update_ok_{true};
};

}  // namespace esphome::ms8607
