// All datasheet page references refer to Bosch Document Number BST-BMP581-DS004-04 (revision number 1.4)

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace bmp581 {

static const uint8_t BMP581_ASIC_ID = 0x50;  // BMP581's ASIC chip ID (page 51 of datasheet)
static const uint8_t RESET_COMMAND = 0xB6;   // Soft reset command

// BMP581 Register Addresses
enum {
  BMP581_CHIP_ID = 0x01,     // read chip ID
  BMP581_INT_SOURCE = 0x15,  // write interrupt sources
  BMP581_MEASUREMENT_DATA =
      0x1D,  // read measurement registers, 0x1D-0x1F are temperature XLSB to MSB and 0x20-0x22 are pressure XLSB to MSB
  BMP581_INT_STATUS = 0x27,  // read interrupt statuses
  BMP581_STATUS = 0x28,      // read sensor status
  BMP581_DSP = 0x30,         // write sensor configuration
  BMP581_DSP_IIR = 0x31,     // write IIR filter configuration
  BMP581_OSR = 0x36,         // write oversampling configuration
  BMP581_ODR = 0x37,         // write data rate and power mode configuration
  BMP581_COMMAND = 0x7E      // write sensor command
};

// BMP581 Power mode operations
enum OperationMode {
  STANDBY_MODE = 0x0,  // no active readings
  NORMAL_MODE = 0x1,   // read continuously at ODR configured rate and standby between
  FORCED_MODE = 0x2,   // read sensor once (only reading mode used by this component)
  NONSTOP_MODE = 0x3   // read continuously with no standby
};

// Temperature and pressure sensors can be oversampled to reduce noise
enum Oversampling {
  OVERSAMPLING_NONE = 0x0,
  OVERSAMPLING_X2 = 0x1,
  OVERSAMPLING_X4 = 0x2,
  OVERSAMPLING_X8 = 0x3,
  OVERSAMPLING_X16 = 0x4,
  OVERSAMPLING_X32 = 0x5,
  OVERSAMPLING_X64 = 0x6,
  OVERSAMPLING_X128 = 0x7
};

// Infinite Impulse Response filter reduces noise caused by ambient disturbances
enum IIRFilter {
  IIR_FILTER_OFF = 0x0,
  IIR_FILTER_2 = 0x1,
  IIR_FILTER_4 = 0x2,
  IIR_FILTER_8 = 0x3,
  IIR_FILTER_16 = 0x4,
  IIR_FILTER_32 = 0x5,
  IIR_FILTER_64 = 0x6,
  IIR_FILTER_128 = 0x7
};

class BMP581Component : public PollingComponent, public i2c::I2CDevice {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }

  void dump_config() override;

  void setup() override;
  void update() override;

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { this->pressure_sensor_ = pressure_sensor; }

  void set_temperature_oversampling_config(Oversampling temperature_oversampling) {
    this->temperature_oversampling_ = temperature_oversampling;
  }
  void set_pressure_oversampling_config(Oversampling pressure_oversampling) {
    this->pressure_oversampling_ = pressure_oversampling;
  }

  void set_temperature_iir_filter_config(IIRFilter iir_temperature_level) {
    this->iir_temperature_level_ = iir_temperature_level;
  }
  void set_pressure_iir_filter_config(IIRFilter iir_pressure_level) { this->iir_pressure_level_ = iir_pressure_level; }

  void set_conversion_time(uint8_t conversion_time) { this->conversion_time_ = conversion_time; }

 protected:
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};

  Oversampling temperature_oversampling_;
  Oversampling pressure_oversampling_;

  IIRFilter iir_temperature_level_;
  IIRFilter iir_pressure_level_;

  // Stores the sensors conversion time needed for a measurement based on oversampling settings and datasheet (page 12)
  // Computed in Python during codegen
  uint8_t conversion_time_;

  // Checks if the BMP581 has measurement data ready by checking the sensor's interrupts
  bool check_data_readiness_();

  // Flushes the IIR filter and primes an initial reading
  bool prime_iir_filter_();

  // Reads temperature data from sensor and converts data to measurement in degrees Celsius
  bool read_temperature_(float &temperature);
  // Reads temperature and pressure data from sensor and converts data to measurements in degrees Celsius and Pa
  bool read_temperature_and_pressure_(float &temperature, float &pressure);

  // Soft resets the BMP581
  bool reset_();

  // Initiates a measurement on sensor by switching to FORCED_MODE
  bool start_measurement_();

  // Writes the IIR filter configuration to the DSP and DSP_IIR registers
  bool write_iir_settings_(IIRFilter temperature_iir, IIRFilter pressure_iir);

  // Writes whether to enable the data ready interrupt to the interrupt source register
  bool write_interrupt_source_settings_(bool data_ready_enable);

  // Writes the oversampling settings to the OSR register
  bool write_oversampling_settings_(Oversampling temperature_oversampling, Oversampling pressure_oversampling);

  // Sets the power mode on the BMP581 by writing to the ODR register
  bool write_power_mode_(OperationMode mode);

  enum ErrorCode {
    NONE = 0,
    ERROR_COMMUNICATION_FAILED,
    ERROR_WRONG_CHIP_ID,
    ERROR_SENSOR_STATUS,
    ERROR_SENSOR_RESET,
    ERROR_PRIME_IIR_FAILED
  } error_code_{NONE};

  // BMP581's interrupt source register (address 0x15) to configure which interrupts are enabled (page 54 of datasheet)
  union {
    struct {
      uint8_t drdy_data_reg_en : 1;  // Data ready interrupt enable
      uint8_t fifo_full_en : 1;      // FIFO full interrupt enable
      uint8_t fifo_ths_en : 1;       // FIFO threshold/watermark interrupt enable
      uint8_t oor_p_en : 1;          // Pressure data out-of-range interrupt enable
    } bit;
    uint8_t reg;
  } int_source_ = {.reg = 0};

  // BMP581's interrupt status register (address 0x27) to determine ensor's current state (page 58 of datasheet)
  union {
    struct {
      uint8_t drdy_data_reg : 1;  // Data ready
      uint8_t fifo_full : 1;      // FIFO full
      uint8_t fifo_ths : 1;       // FIFO fhreshold/watermark
      uint8_t oor_p : 1;          // Pressure data out-of-range
      uint8_t por : 1;            // Power-On-Reset complete
    } bit;
    uint8_t reg;
  } int_status_ = {.reg = 0};

  // BMP581's status register (address 0x28) to determine if sensor has setup correctly (page 58 of datasheet)
  union {
    struct {
      uint8_t status_core_rdy : 1;
      uint8_t status_nvm_rdy : 1;             // asserted if NVM is ready of operations
      uint8_t status_nvm_err : 1;             // asserted if NVM error
      uint8_t status_nvm_cmd_err : 1;         // asserted if boot command error
      uint8_t status_boot_err_corrected : 1;  // asserted if a boot error has been corrected
      uint8_t : 2;
      uint8_t st_crack_pass : 1;  // asserted if crack check has executed without detecting a crack
    } bit;
    uint8_t reg;
  } status_ = {.reg = 0};

  // BMP581's dsp register (address 0x30) to configure data registers iir selection (page 61 of datasheet)
  union {
    struct {
      uint8_t comp_pt_en : 2;           // enable temperature and pressure compensation
      uint8_t iir_flush_forced_en : 1;  // IIR filter is flushed in forced mode
      uint8_t shdw_sel_iir_t : 1;       // temperature data register value selected before or after iir
      uint8_t fifo_sel_iir_t : 1;       // FIFO temperature data register value secected before or after iir
      uint8_t shdw_sel_iir_p : 1;       // pressure data register value selected before or after iir
      uint8_t fifo_sel_iir_p : 1;       // FIFO pressure data register value selected before or after iir
      uint8_t oor_sel_iir_p : 1;        // pressure out-of-range value selected before or after iir
    } bit;
    uint8_t reg;
  } dsp_config_ = {.reg = 0};

  // BMP581's iir register (address 0x31) to configure iir filtering(page 62 of datasheet)
  union {
    struct {
      uint8_t set_iir_t : 3;  // Temperature IIR filter coefficient
      uint8_t set_iir_p : 3;  // Pressure IIR filter coefficient
    } bit;
    uint8_t reg;
  } iir_config_ = {.reg = 0};

  // BMP581's OSR register (address 0x36) to configure Over-Sampling Rates (page 64 of datasheet)
  union {
    struct {
      uint8_t osr_t : 3;     // Temperature oversampling
      uint8_t osr_p : 3;     // Pressure oversampling
      uint8_t press_en : 1;  // Enables pressure measurement
    } bit;
    uint8_t reg;
  } osr_config_ = {.reg = 0};

  // BMP581's odr register (address 0x37) to configure output data rate and power mode (page 64 of datasheet)
  union {
    struct {
      uint8_t pwr_mode : 2;  // power mode of sensor
      uint8_t odr : 5;       // output data rate
      uint8_t deep_dis : 1;  // deep standby disabled if asserted
    } bit;
    uint8_t reg;
  } odr_config_ = {.reg = 0};
};

}  // namespace bmp581
}  // namespace esphome
