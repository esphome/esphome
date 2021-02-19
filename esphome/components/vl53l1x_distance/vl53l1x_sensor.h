#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "vl53l1_error_codes.h"

// imported from vl531lx_class.h
static const uint8_t VL53L1X_IMPLEMENTATION_VER_MAJOR = 1;
static const uint8_t VL53L1X_IMPLEMENTATION_VER_MINOR = 0;
static const uint8_t VL53L1X_IMPLEMENTATION_VER_SUB = 1;
static const uint8_t VL53L1X_IMPLEMENTATION_VER_REVISION = 0000;

typedef int8_t VL53L1X_ERROR;

static const uint8_t SOFT_RESET = 0x0000;
static const uint8_t VL53L1_I2C_SLAVE__DEVICE_ADDRESS = 0x0001;
static const uint8_t VL53L1_VHV_CONFIG__TIMEOUT_MACROP_LOOP_BOUND = 0x0008;
static const uint8_t ALGO__CROSSTALK_COMPENSATION_PLANE_OFFSET_KCPS = 0x0016;
static const uint8_t ALGO__CROSSTALK_COMPENSATION_X_PLANE_GRADIENT_KCPS = 0x0018;
static const uint8_t ALGO__CROSSTALK_COMPENSATION_Y_PLANE_GRADIENT_KCPS = 0x001A;
static const uint8_t ALGO__PART_TO_PART_RANGE_OFFSET_MM = 0x001E;
static const uint8_t MM_CONFIG__INNER_OFFSET_MM = 0x0020;
static const uint8_t MM_CONFIG__OUTER_OFFSET_MM = 0x0022;
static const uint8_t GPIO_HV_MUX__CTRL = 0x0030;
static const uint8_t GPIO__TIO_HV_STATUS = 0x0031;
static const uint8_t SYSTEM__INTERRUPT_CONFIG_GPIO = 0x0046;
static const uint8_t PHASECAL_CONFIG__TIMEOUT_MACROP = 0x004B;
static const uint8_t RANGE_CONFIG__TIMEOUT_MACROP_A_HI = 0x005E;
static const uint8_t RANGE_CONFIG__VCSEL_PERIOD_A = 0x0060;
static const uint8_t RANGE_CONFIG__VCSEL_PERIOD_B = 0x0063;
static const uint8_t RANGE_CONFIG__TIMEOUT_MACROP_B_HI = 0x0061;
static const uint8_t RANGE_CONFIG__TIMEOUT_MACROP_B_LO = 0x0062;
static const uint8_t RANGE_CONFIG__SIGMA_THRESH = 0x0064;
static const uint8_t RANGE_CONFIG__MIN_COUNT_RATE_RTN_LIMIT_MCPS = 0x0066;
static const uint8_t RANGE_CONFIG__VALID_PHASE_HIGH = 0x0069;
static const uint8_t VL53L1_SYSTEM__INTERMEASUREMENT_PERIOD = 0x006C;
static const uint8_t SYSTEM__THRESH_HIGH = 0x0072;
static const uint8_t SYSTEM__THRESH_LOW = 0x0074;
static const uint8_t SD_CONFIG__WOI_SD0 = 0x0078;
static const uint8_t SD_CONFIG__INITIAL_PHASE_SD0 = 0x007A;
static const uint8_t ROI_CONFIG__USER_ROI_CENTRE_SPAD = 0x007F;
static const uint8_t ROI_CONFIG__USER_ROI_REQUESTED_GLOBAL_XY_SIZE = 0x0080;
static const uint8_t SYSTEM__SEQUENCE_CONFIG = 0x0081;
static const uint8_t VL53L1_SYSTEM__GROUPED_PARAMETER_HOLD = 0x0082;
static const uint8_t SYSTEM__INTERRUPT_CLEAR = 0x0086;
static const uint8_t SYSTEM__MODE_START = 0x0087;
static const uint8_t VL53L1_RESULT__RANGE_STATUS = 0x0089;
static const uint8_t VL53L1_RESULT__DSS_ACTUAL_EFFECTIVE_SPADS_SD0 = 0x008C;
static const uint8_t RESULT__AMBIENT_COUNT_RATE_MCPS_SD = 0x0090;
static const uint8_t VL53L1_RESULT__FINAL_CROSSTALK_CORRECTED_RANGE_MM_SD0 = 0x0096;
static const uint8_t VL53L1_RESULT__PEAK_SIGNAL_COUNT_RATE_CROSSTALK_CORRECTED_MCPS_SD0 = 0x0098;
static const uint8_t VL53L1_RESULT__OSC_CALIBRATE_VAL = 0x00DE;
static const uint8_t VL53L1_FIRMWARE__SYSTEM_STATUS = 0x00E5;
static const uint8_t VL53L1_IDENTIFICATION__MODEL_ID = 0x010F;
static const uint8_t VL53L1_ROI_CONFIG__MODE_ROI_CENTRE_SPAD = 0x013E;

static const uint8_t VL53L1X_DEFAULT_DEVICE_ADDRESS = 0x52;

// imported from vl531lx_class.cpp
const uint8_t VL51L1X_DEFAULT_CONFIGURATION[] = {
    0x00, /* 0x2d : set bit 2 and 5 to 1 for fast plus mode (1MHz I2C), else don't touch */
    0x01, /* 0x2e : bit 0 if I2C pulled up at 1.8V, else set bit 0 to 1 (pull up at AVDD) */
    0x01, /* 0x2f : bit 0 if GPIO pulled up at 1.8V, else set bit 0 to 1 (pull up at AVDD) */
    0x01, /* 0x30 : set bit 4 to 0 for active high interrupt and 1 for active low (bits 3:0 must be 0x1), use
             SetInterruptPolarity() */
    0x02, /* 0x31 : bit 1 = interrupt depending on the polarity, use CheckForDataReady() */
    0x00, /* 0x32 : not user-modifiable */
    0x02, /* 0x33 : not user-modifiable */
    0x08, /* 0x34 : not user-modifiable */
    0x00, /* 0x35 : not user-modifiable */
    0x08, /* 0x36 : not user-modifiable */
    0x10, /* 0x37 : not user-modifiable */
    0x01, /* 0x38 : not user-modifiable */
    0x01, /* 0x39 : not user-modifiable */
    0x00, /* 0x3a : not user-modifiable */
    0x00, /* 0x3b : not user-modifiable */
    0x00, /* 0x3c : not user-modifiable */
    0x00, /* 0x3d : not user-modifiable */
    0xff, /* 0x3e : not user-modifiable */
    0x00, /* 0x3f : not user-modifiable */
    0x0F, /* 0x40 : not user-modifiable */
    0x00, /* 0x41 : not user-modifiable */
    0x00, /* 0x42 : not user-modifiable */
    0x00, /* 0x43 : not user-modifiable */
    0x00, /* 0x44 : not user-modifiable */
    0x00, /* 0x45 : not user-modifiable */
    0x20, /* 0x46 : interrupt configuration 0->level low detection, 1-> level high, 2-> Out of window, 3->In window,
             0x20-> New sample ready , TBC */
    0x0b, /* 0x47 : not user-modifiable */
    0x00, /* 0x48 : not user-modifiable */
    0x00, /* 0x49 : not user-modifiable */
    0x02, /* 0x4a : not user-modifiable */
    0x0a, /* 0x4b : not user-modifiable */
    0x21, /* 0x4c : not user-modifiable */
    0x00, /* 0x4d : not user-modifiable */
    0x00, /* 0x4e : not user-modifiable */
    0x05, /* 0x4f : not user-modifiable */
    0x00, /* 0x50 : not user-modifiable */
    0x00, /* 0x51 : not user-modifiable */
    0x00, /* 0x52 : not user-modifiable */
    0x00, /* 0x53 : not user-modifiable */
    0xc8, /* 0x54 : not user-modifiable */
    0x00, /* 0x55 : not user-modifiable */
    0x00, /* 0x56 : not user-modifiable */
    0x38, /* 0x57 : not user-modifiable */
    0xff, /* 0x58 : not user-modifiable */
    0x01, /* 0x59 : not user-modifiable */
    0x00, /* 0x5a : not user-modifiable */
    0x08, /* 0x5b : not user-modifiable */
    0x00, /* 0x5c : not user-modifiable */
    0x00, /* 0x5d : not user-modifiable */
    0x01, /* 0x5e : not user-modifiable */
    0xdb, /* 0x5f : not user-modifiable */
    0x0f, /* 0x60 : not user-modifiable */
    0x01, /* 0x61 : not user-modifiable */
    0xf1, /* 0x62 : not user-modifiable */
    0x0d, /* 0x63 : not user-modifiable */
    0x01, /* 0x64 : Sigma threshold MSB (mm in 14.2 format for MSB+LSB), use SetSigmaThreshold(), default value 90
             mm  */
    0x68, /* 0x65 : Sigma threshold LSB */
    0x00, /* 0x66 : Min count Rate MSB (MCPS in 9.7 format for MSB+LSB), use SetSignalThreshold() */
    0x80, /* 0x67 : Min count Rate LSB */
    0x08, /* 0x68 : not user-modifiable */
    0xb8, /* 0x69 : not user-modifiable */
    0x00, /* 0x6a : not user-modifiable */
    0x00, /* 0x6b : not user-modifiable */
    0x00, /* 0x6c : Intermeasurement period MSB, 32 bits register, use SetIntermeasurementInMs() */
    0x00, /* 0x6d : Intermeasurement period */
    0x0f, /* 0x6e : Intermeasurement period */
    0x89, /* 0x6f : Intermeasurement period LSB */
    0x00, /* 0x70 : not user-modifiable */
    0x00, /* 0x71 : not user-modifiable */
    0x00, /* 0x72 : distance threshold high MSB (in mm, MSB+LSB), use SetD:tanceThreshold() */
    0x00, /* 0x73 : distance threshold high LSB */
    0x00, /* 0x74 : distance threshold low MSB ( in mm, MSB+LSB), use SetD:tanceThreshold() */
    0x00, /* 0x75 : distance threshold low LSB */
    0x00, /* 0x76 : not user-modifiable */
    0x01, /* 0x77 : not user-modifiable */
    0x0f, /* 0x78 : not user-modifiable */
    0x0d, /* 0x79 : not user-modifiable */
    0x0e, /* 0x7a : not user-modifiable */
    0x0e, /* 0x7b : not user-modifiable */
    0x00, /* 0x7c : not user-modifiable */
    0x00, /* 0x7d : not user-modifiable */
    0x02, /* 0x7e : not user-modifiable */
    0xc7, /* 0x7f : ROI center, use SetROI() */
    0xff, /* 0x80 : XY ROI (X=Width, Y=Height), use SetROI() */
    0x9B, /* 0x81 : not user-modifiable */
    0x00, /* 0x82 : not user-modifiable */
    0x00, /* 0x83 : not user-modifiable */
    0x00, /* 0x84 : not user-modifiable */
    0x01, /* 0x85 : not user-modifiable */
    0x00, /* 0x86 : clear interrupt, use ClearInterrupt() */
    0x00  /* 0x87 : start ranging, use StartRanging() or StopRanging(), If you want an automatic start after
             VL53L1X_init() call, put 0x40 in location 0x87 */
};
//

namespace esphome {
namespace vl53l1x {

enum DistanceMode { SHORT, MEDIUM, LONG };

class VL53L1XSensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void update() override;

  void loop() override;

  // Esphome config SETTERS
  void set_timing_budget(uint32_t budget) { timing_budget_ = budget; }
  void set_distance_mode(DistanceMode mode) { distance_mode_ = mode; }

  // void set_retry_budget(uint8_t budget) { retry_budget_ = budget; }

  // VL53L1X booleans and function
  bool begin();     // Initialization of sensor
  bool check_id();  // Check the ID of the sensor, returns true if ID is correct

  void start_ranging();                                   // Begins taking measurements
  void stop_ranging();                                    // Stops taking measurements
  bool check_for_data_ready(uint8_t isdataReady);         // Checks the to see if data is ready
  int8_t apply_timing_budget_in_ms(uint16_t timingBudget);  // Set the timing budget for a measurement
  uint16_t get_timing_budget_in_ms();                     // Get the timing budget for a measurement
  int8_t apply_distance_mode(DistanceMode mode);
  int8_t apply_distance_mode_long();   // Set to 4M range
  int8_t apply_distance_mode_short();  // Set to 1.3M range

  uint16_t get_distance();  // Returns distance

  // ROI
  uint8_t apply_ROI(
      uint8_t X, uint8_t Y,
      uint8_t opticalCenter);   // Set the height and width of the ROI(region of interest) in SPADs, lowest possible
                                // option is 4. Set optical center based on above table
  uint16_t get_ROIX();  // Returns the width of the ROI in SPADs
  uint16_t get_ROIY();  // Returns the height of the ROI in SPADs

 protected:
  DistanceMode distance_mode_{DistanceMode::LONG};
  uint32_t timing_budget_{50};
  /*   TO DO: retries */
  //   uint8_t retry_budget_{5};
  //   uint8_t retry_count_{0};
};

}  // namespace vl53l1x
}  // namespace esphome
