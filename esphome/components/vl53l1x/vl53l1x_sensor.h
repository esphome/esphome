#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace vl53l1x {

// Distance modes - affect maximum range and accuracy
enum DistanceMode : uint8_t {
  DISTANCE_MODE_SHORT = 1,   // Up to 1.3m, better ambient immunity
  DISTANCE_MODE_MEDIUM = 2,  // Up to 3m
  DISTANCE_MODE_LONG = 3,    // Up to 4m, more affected by ambient light
};

// Measurement state machine
enum MeasurementState : uint8_t {
  MEASUREMENT_STATE_IDLE,              // No measurement in progress
  MEASUREMENT_STATE_WAITING_FOR_DATA,  // Measurement started, polling for completion
};

// VL53L1X register addresses (16-bit)
// Based on Pololu VL53L1X library - https://github.com/pololu/vl53l1x-arduino
enum VL53L1XRegister : uint16_t {
  // Identification
  IDENTIFICATION_MODEL_ID = 0x010F,  // Expected: 0xEACC

  // System control
  SOFT_RESET = 0x0000,
  I2C_DEVICE_ADDRESS = 0x0001,

  // I/O configuration
  PAD_I2C_HV_EXTSUP_CONFIG = 0x002E,

  // Interrupt configuration
  GPIO_HV_MUX_CTRL = 0x0030,
  GPIO_TIO_HV_STATUS = 0x0031,
  SYSTEM_INTERRUPT_CLEAR = 0x0086,

  // Firmware status
  FIRMWARE_SYSTEM_STATUS = 0x00E5,

  // Range configuration
  RANGE_CONFIG_VCSEL_PERIOD_A = 0x0060,
  RANGE_CONFIG_VCSEL_PERIOD_B = 0x0063,
  RANGE_CONFIG_TIMEOUT_MACROP_A_HI = 0x005E,
  RANGE_CONFIG_TIMEOUT_MACROP_B_HI = 0x0061,
  RANGE_CONFIG_VALID_PHASE_HIGH = 0x0069,
  RANGE_CONFIG_SIGMA_THRESH = 0x0064,
  RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT_MCPS = 0x0066,

  // Timing configuration
  SYSTEM_INTERMEASUREMENT_PERIOD = 0x006C,

  // DSS (Dynamic SPAD Selection) configuration
  DSS_CONFIG_TARGET_TOTAL_RATE_MCPS = 0x0024,
  DSS_CONFIG_MANUAL_EFFECTIVE_SPADS_SELECT = 0x0054,
  DSS_CONFIG_ROI_MODE_CONTROL = 0x004F,
  DSS_CONFIG_APERTURE_ATTENUATION = 0x0057,

  // ROI configuration
  ROI_CONFIG_USER_ROI_CENTRE_SPAD = 0x007F,
  ROI_CONFIG_USER_ROI_REQUESTED_GLOBAL_XY_SIZE = 0x0080,

  // Oscillator
  OSC_MEASURED_FAST_OSC_FREQUENCY = 0x0006,
  VHV_CONFIG_TIMEOUT_MACROP_LOOP_BOUND = 0x0008,
  VHV_CONFIG_INIT = 0x000B,

  // Sequence configuration
  SD_CONFIG_WOI_SD0 = 0x0078,
  SD_CONFIG_WOI_SD1 = 0x0079,
  SD_CONFIG_INITIAL_PHASE_SD0 = 0x007A,
  SD_CONFIG_INITIAL_PHASE_SD1 = 0x007B,

  // System control
  SYSTEM_SEQUENCE_CONFIG = 0x0081,
  SYSTEM_MODE_START = 0x0087,

  // Calibration
  RESULT_OSC_CALIBRATE_VAL = 0x00DE,
  PHASECAL_CONFIG_TIMEOUT_MACROP = 0x004B,
  PHASECAL_CONFIG_OVERRIDE = 0x004D,
  CAL_CONFIG_VCSEL_START = 0x0047,

  // Results
  RESULT_RANGE_STATUS = 0x0089,
  RESULT_DSS_ACTUAL_EFFECTIVE_SPADS_SD0 = 0x008C,
  RESULT_AMBIENT_COUNT_RATE_MCPS_SD = 0x0090,
  RESULT_FINAL_CROSSTALK_CORRECTED_RANGE_MM_SD0 = 0x0096,
  RESULT_PEAK_SIGNAL_COUNT_RATE_CROSSTALK_CORRECTED_MCPS_SD0 = 0x0098,

  // Offset calibration
  ALGO_PART_TO_PART_RANGE_OFFSET_MM = 0x001E,
  MM_CONFIG_INNER_OFFSET_MM = 0x0020,
  MM_CONFIG_OUTER_OFFSET_MM = 0x0022,
};

// Range status values from measurement
enum class RangeStatus : uint8_t {
  RANGE_VALID = 0,
  SIGMA_FAIL = 1,
  SIGNAL_FAIL = 2,
  RANGE_VALID_MIN_RANGE_CLIPPED = 3,
  OUT_OF_BOUNDS_FAIL = 4,
  HARDWARE_FAIL = 5,
  RANGE_VALID_NO_WRAP_CHECK_FAIL = 6,
  WRAP_TARGET_FAIL = 7,
  PROCESSING_FAIL = 8,
  CROSSTALK_SIGNAL_FAIL = 9,
  SYNC_FAIL = 10,
  RANGE_VALID_MERGED_PULSE = 11,
  TARGET_PRESENT_LACK_OF_SIGNAL = 12,
  MIN_RANGE_FAIL = 13,
  RANGE_INVALID = 14,
  NONE = 255,
};

class VL53L1XSensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Configuration setters (called by generated code)
  void set_distance_mode(DistanceMode mode) { this->distance_mode_ = mode; }
  void set_timing_budget(uint32_t budget_us) { this->timing_budget_us_ = budget_us; }
  void set_timeout(uint32_t timeout_us) { this->timeout_us_ = timeout_us; }
  void set_enable_pin(GPIOPin *pin) { this->enable_pin_ = pin; }
  void set_signal_rate_limit(float limit) { this->signal_rate_limit_ = limit; }
  void set_roi(uint8_t width, uint8_t height, uint8_t center);
  void set_offset(int16_t offset_mm) { this->offset_mm_ = offset_mm; }

 protected:
  // Initialization helpers
  bool init_sensor_();
  bool wait_for_boot_();
  bool configure_sensor_();
  bool set_distance_mode_internal_(DistanceMode mode);
  bool set_timing_budget_internal_(uint32_t budget_us);
  bool set_roi_internal_();
  bool set_offset_internal_();

  // Measurement helpers
  bool start_measurement_();
  bool data_ready_();
  bool read_results_();
  void clear_interrupt_();

  // I2C helpers for 16-bit register addresses
  bool write_reg_(uint16_t reg, uint8_t value);
  bool write_reg16_(uint16_t reg, uint16_t value);
  bool write_reg32_(uint16_t reg, uint32_t value);
  bool read_reg_(uint16_t reg, uint8_t *value);
  bool read_reg16_(uint16_t reg, uint16_t *value);

  // Timing calculation helpers
  uint32_t calc_macro_period_(uint8_t vcsel_period);
  uint32_t timeout_mclks_to_microseconds_(uint32_t mclks, uint32_t macro_period_us);
  uint32_t timeout_microseconds_to_mclks_(uint32_t timeout_us, uint32_t macro_period_us);
  uint16_t encode_timeout_(uint32_t mclks);
  uint32_t decode_timeout_(uint16_t encoded);

  // Configuration
  DistanceMode distance_mode_{DISTANCE_MODE_LONG};
  uint32_t timing_budget_us_{50000};
  uint32_t timeout_us_{100000};
  GPIOPin *enable_pin_{nullptr};
  float signal_rate_limit_{0.25f};
  uint8_t roi_width_{16};
  uint8_t roi_height_{16};
  uint8_t roi_center_{199};
  int16_t offset_mm_{0};

  // State
  MeasurementState state_{MEASUREMENT_STATE_IDLE};
  uint32_t measurement_start_us_{0};

  // Calibration data read during init
  uint16_t fast_osc_frequency_{0};
  uint16_t osc_calibrate_val_{0};

  // VHV calibration values (saved for each measurement)
  uint8_t saved_vhv_init_{0};
  uint8_t saved_vhv_timeout_{0};

  // Timing guard for measurement budget calculation
  static constexpr uint32_t TIMING_GUARD_US = 4528;
  static constexpr uint16_t TARGET_RATE = 0x0A00;
};

}  // namespace vl53l1x
}  // namespace esphome
