#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ms8607 {

/**
 Class for I2CDevice used to communicate with the Humidity sensor
 on the chip. See MS8607Component instead
 */
class MS8607HumidityDevice : public i2c::I2CDevice {
 public:
  uint8_t get_address() { return address_; }
};

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
class MS8607Component : public PollingComponent, public i2c::I2CDevice {
 public:
  virtual ~MS8607Component() = default;
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; };

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { pressure_sensor_ = pressure_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }
  void set_humidity_device(MS8607HumidityDevice *humidity_device) { humidity_device_ = humidity_device; }

 protected:
  /**
   Read and store the Pressure & Temperature calibration settings from the PROM.
   Intended to be called during setup(), this will set the `failure_reason_`
   */
  bool read_calibration_values_from_prom_();

  /// Start async temperature read
  void request_read_temperature_();
  /// Process async temperature read
  void read_temperature_();
  /// start async pressure read
  void request_read_pressure_(uint32_t raw_temperature);
  /// process async pressure read
  void read_pressure_(uint32_t raw_temperature);
  /// start async humidity read
  void request_read_humidity_(float temperature_float);
  /// process async humidity read
  void read_humidity_(float temperature_float);
  /// use raw temperature & pressure to calculate & publish values
  void calculate_values_(uint32_t raw_temperature, uint32_t raw_pressure);

  sensor::Sensor *temperature_sensor_;
  sensor::Sensor *pressure_sensor_;
  sensor::Sensor *humidity_sensor_;

  /** I2CDevice object to communicate with secondary I2C address for the humidity sensor
   *
   * The MS8607 only has one set of I2C pins, despite using two different addresses.
   *
   * Default address for humidity is 0x40
   */
  MS8607HumidityDevice *humidity_device_;

  /// This device's pressure & temperature calibration values, read from PROM
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
  } calibration_values_;

  /// Possible failure reasons of this component
  enum class ErrorCode;
  /// Keep track of the reason why this component failed, to augment the dumped config
  ErrorCode error_code_;

  /// Current progress through required component setup
  enum class SetupStatus;
  /// Current step in the multi-step & possibly delayed setup() process
  SetupStatus setup_status_;
};

}  // namespace ms8607
}  // namespace esphome
