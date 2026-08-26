#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::ms8607 {

/**
 Class for I2CDevice used to communicate with the Humidity sensor
 on the chip. See MS8607Component instead
 */
class MS8607HumidityDevice final : public i2c::I2CDevice {
 public:
  uint8_t get_address() const { return this->address_; }
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
class MS8607Component final : public PollingComponent, public i2c::I2CDevice {
 public:
  ~MS8607Component() = default;
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { pressure_sensor_ = pressure_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }
  void set_humidity_device(MS8607HumidityDevice *humidity_device) { humidity_device_ = humidity_device; }

 protected:
  /// Attempt to reset both I2C devices, retrying with backoff on failure
  void try_reset_();
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

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};

  /** I2CDevice object to communicate with secondary I2C address for the humidity sensor
   *
   * The MS8607 only has one set of I2C pins, despite using two different addresses.
   *
   * Default address for humidity is 0x40
   */
  MS8607HumidityDevice *humidity_device_{nullptr};

 public:  // for testability
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
  } calibration_values_{};

 protected:
  /// Possible failure reasons of this component
  enum class ErrorCode {
    /// Component hasn't failed (yet?)
    ERROR_CODE_NONE = 0,
    /// Both the Pressure/Temperature address and the Humidity address failed to reset
    ERROR_CODE_PTH_RESET_FAILED = 1,
    /// Asking the Pressure/Temperature sensor to reset failed
    ERROR_CODE_PT_RESET_FAILED = 2,
    /// Asking the Humidity sensor to reset failed
    ERROR_CODE_H_RESET_FAILED = 3,
    /// Reading the PROM calibration values failed
    ERROR_CODE_PROM_READ_FAILED = 4,
    /// The PROM calibration values failed the CRC check
    ERROR_CODE_PROM_CRC_FAILED = 5,
  };

  /// Current progress through required component setup
  enum class SetupStatus {
    /// This component has not successfully reset the PT & H devices
    SETUP_STATUS_NEEDS_RESET,
    /// Reset commands succeeded, need to wait >= 15ms to read PROM
    SETUP_STATUS_NEEDS_PROM_READ,
    /// Successfully read PROM and ready to update sensors
    SETUP_STATUS_SUCCESSFUL,
  };

  /// Keep track of the reason why this component failed, to augment the dumped config
  ErrorCode error_code_{ErrorCode::ERROR_CODE_NONE};

  /// Current step in the multi-step & possibly delayed setup() process
  SetupStatus setup_status_{SetupStatus::SETUP_STATUS_NEEDS_RESET};
  uint32_t reset_interval_{5};
  uint8_t reset_attempts_remaining_{3};

 public:  // for testability
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
  static struct CompensatedTemperature compensated_temperature(
      uint32_t d2_raw_temperature, const struct MS8607Component::CalibrationValues &calibration_values);

  /// use raw pressure, calibration values, and current temperature to figure out actual pressure
  static float compensated_pressure(uint32_t d1_raw_pressure, const struct CalibrationValues &calibration_values,
                                    const struct CompensatedTemperature &temperature_values);

  /// convert raw humidity value into correct range & apply temperature compensation calculation
  static float compensated_humidity(float humidity_float, float temperature_float);
};

}  // namespace esphome::ms8607
