/*
  based on BMP388_DEV by Martin Lindupp
  under MIT License (MIT)
  Copyright (C) Martin Lindupp 2020
  http://github.com/MartinL1/BMP388_DEV
*/

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace bmp3xx {

static const uint8_t BMP388_ID = 0x50;   // The BMP388 device ID
static const uint8_t BMP390_ID = 0x60;   // The BMP390 device ID
static const uint8_t RESET_CODE = 0xB6;  // The BMP388 reset code

/// BMP388_DEV Registers
enum {
  BMP388_CHIP_ID = 0x00,       // Chip ID register sub-address
  BMP388_ERR_REG = 0x02,       // Error register sub-address
  BMP388_STATUS = 0x03,        // Status register sub-address
  BMP388_DATA_0 = 0x04,        // Pressure eXtended Least Significant Byte (XLSB) register sub-address
  BMP388_DATA_1 = 0x05,        // Pressure Least Significant Byte (LSB) register sub-address
  BMP388_DATA_2 = 0x06,        // Pressure Most Significant Byte (MSB) register sub-address
  BMP388_DATA_3 = 0x07,        // Temperature eXtended Least Significant Byte (XLSB) register sub-address
  BMP388_DATA_4 = 0x08,        // Temperature Least Significant Byte (LSB) register sub-address
  BMP388_DATA_5 = 0x09,        // Temperature Most Significant Byte (MSB) register sub-address
  BMP388_SENSORTIME_0 = 0x0C,  // Sensor time register 0 sub-address
  BMP388_SENSORTIME_1 = 0x0D,  // Sensor time register 1 sub-address
  BMP388_SENSORTIME_2 = 0x0E,  // Sensor time register 2 sub-address
  BMP388_EVENT = 0x10,         // Event register sub-address
  BMP388_INT_STATUS = 0x11,    // Interrupt Status register sub-address
  BMP388_INT_CTRL = 0x19,      // Interrupt Control register sub-address
  BMP388_IF_CONFIG = 0x1A,     // Interface Configuration register sub-address
  BMP388_PWR_CTRL = 0x1B,      // Power Control register sub-address
  BMP388_OSR = 0x1C,           // Oversampling register sub-address
  BMP388_ODR = 0x1D,           // Output Data Rate register sub-address
  BMP388_CONFIG = 0x1F,        // Configuration register sub-address
  BMP388_TRIM_PARAMS = 0x31,   // Trim parameter registers' base sub-address
  BMP388_CMD = 0x7E            // Command register sub-address
};

/// Device mode bitfield in the control and measurement register
enum OperationMode { SLEEP_MODE = 0x00, FORCED_MODE = 0x01, NORMAL_MODE = 0x03 };

/// Oversampling bit fields in the control and measurement register
enum Oversampling {
  OVERSAMPLING_NONE = 0x00,
  OVERSAMPLING_X2 = 0x01,
  OVERSAMPLING_X4 = 0x02,
  OVERSAMPLING_X8 = 0x03,
  OVERSAMPLING_X16 = 0x04,
  OVERSAMPLING_X32 = 0x05
};

/// Infinite Impulse Response (IIR) filter bit field in the configuration register
enum IIRFilter {
  IIR_FILTER_OFF = 0x00,
  IIR_FILTER_2 = 0x01,
  IIR_FILTER_4 = 0x02,
  IIR_FILTER_8 = 0x03,
  IIR_FILTER_16 = 0x04,
  IIR_FILTER_32 = 0x05,
  IIR_FILTER_64 = 0x06,
  IIR_FILTER_128 = 0x07
};

/// This class implements support for the BMP3XX Temperature+Pressure i2c sensor.
class BMP3XXComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { pressure_sensor_ = pressure_sensor; }

  /// Set the oversampling value for the temperature sensor. Default is 16x.
  void set_temperature_oversampling_config(Oversampling temperature_oversampling) {
    this->temperature_oversampling_ = temperature_oversampling;
  }
  /// Set the oversampling value for the pressure sensor. Default is 16x.
  void set_pressure_oversampling_config(Oversampling pressure_oversampling) {
    this->pressure_oversampling_ = pressure_oversampling;
  }
  /// Set the IIR Filter used to increase accuracy, defaults to no IIR Filter.
  void set_iir_filter_config(IIRFilter iir_filter) { this->iir_filter_ = iir_filter; }

  /// Soft reset the sensor
  uint8_t reset();
  /// Start continuous measurement in NORMAL_MODE
  bool start_normal_conversion();
  /// Start a one shot measurement in FORCED_MODE
  bool start_forced_conversion();
  /// Stop the conversion and return to SLEEP_MODE
  bool stop_conversion();
  /// Set the pressure oversampling: OFF, X1, X2, X4, X8, X16, X32
  bool set_pressure_oversampling(Oversampling pressure_oversampling);
  /// Set the temperature oversampling: OFF, X1, X2, X4, X8, X16, X32
  bool set_temperature_oversampling(Oversampling temperature_oversampling);
  /// Set the IIR filter setting: OFF, 2, 3, 8, 16, 32
  bool set_iir_filter(IIRFilter iir_filter);
  /// Get a temperature measurement
  bool get_temperature(float &temperature);
  /// Get a pressure measurement
  bool get_pressure(float &pressure);
  /// Get a temperature and pressure measurement
  bool get_measurements(float &temperature, float &pressure);
  /// Get a temperature and pressure measurement
  bool get_measurement();
  /// Set the barometer mode
  bool set_mode(OperationMode mode);
  /// Set the BMP388 oversampling register
  bool set_oversampling_register(Oversampling pressure_oversampling, Oversampling temperature_oversampling);
  /// Checks if a measurement is ready
  bool data_ready();

 protected:
  Oversampling temperature_oversampling_{OVERSAMPLING_X16};
  Oversampling pressure_oversampling_{OVERSAMPLING_X16};
  IIRFilter iir_filter_{IIR_FILTER_OFF};
  OperationMode operation_mode_{FORCED_MODE};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  enum ErrorCode {
    NONE = 0,
    ERROR_COMMUNICATION_FAILED,
    ERROR_WRONG_CHIP_ID,
    ERROR_SENSOR_STATUS,
    ERROR_SENSOR_RESET,
  } error_code_{NONE};

  struct {  // The BMP388 compensation trim parameters (coefficients)
    uint16_t param_T1;
    uint16_t param_T2;
    int8_t param_T3;
    int16_t param_P1;
    int16_t param_P2;
    int8_t param_P3;
    int8_t param_P4;
    uint16_t param_P5;
    uint16_t param_P6;
    int8_t param_P7;
    int8_t param_P8;
    int16_t param_P9;
    int8_t param_P10;
    int8_t param_P11;
  } __attribute__((packed)) compensation_params_;

  struct FloatParams {  // The BMP388 float point compensation trim parameters
    float param_T1;
    float param_T2;
    float param_T3;
    float param_P1;
    float param_P2;
    float param_P3;
    float param_P4;
    float param_P5;
    float param_P6;
    float param_P7;
    float param_P8;
    float param_P9;
    float param_P10;
    float param_P11;
  } compensation_float_params_;

  union {  // Copy of the BMP388's chip id register
    struct {
      uint8_t chip_id_nvm : 4;
      uint8_t chip_id_fixed : 4;
    } bit;
    uint8_t reg;
  } chip_id_ = {.reg = 0};

  union {  // Copy of the BMP388's event register
    struct {
      uint8_t por_detected : 1;
    } bit;
    uint8_t reg;
  } event_ = {.reg = 0};

  union {  // Copy of the BMP388's interrupt status register
    struct {
      uint8_t fwm_int : 1;
      uint8_t ffull_int : 1;
      uint8_t : 1;
      uint8_t drdy : 1;
    } bit;
    uint8_t reg;
  } int_status_ = {.reg = 0};

  union {  // Copy of the BMP388's power control register
    struct {
      uint8_t press_en : 1;
      uint8_t temp_en : 1;
      uint8_t : 2;
      uint8_t mode : 2;
    } bit;
    uint8_t reg;
  } pwr_ctrl_ = {.reg = 0};

  union {  // Copy of the BMP388's oversampling register
    struct {
      uint8_t osr_p : 3;
      uint8_t osr_t : 3;
    } bit;
    uint8_t reg;
  } osr_ = {.reg = 0};

  union {  // Copy of the BMP388's output data rate register
    struct {
      uint8_t odr_sel : 5;
    } bit;
    uint8_t reg;
  } odr_ = {.reg = 0};

  union {  // Copy of the BMP388's configuration register
    struct {
      uint8_t : 1;
      uint8_t iir_filter : 3;
    } bit;
    uint8_t reg;
  } config_ = {.reg = 0};

  // Bosch temperature compensation function
  float bmp388_compensate_temperature_(float uncomp_temp);
  // Bosch pressure compensation function
  float bmp388_compensate_pressure_(float uncomp_press, float t_lin);
};

}  // namespace bmp3xx
}  // namespace esphome
