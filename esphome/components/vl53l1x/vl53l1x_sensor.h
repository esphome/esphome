/*
 * Most of the code in this integration is based on the Adafruit_VL53L1X library
 * by Adafruit Industries, which in turn is based on
 * the VL53L1X API provided by ST (STSW-IMG007).
 *
 * For more information about licensing, please view the included LICENSE.txt file
 * in the vl53l1x integration directory.
 */

#pragma once

#include <list>

#include <esphome/core/hal.h>
#include <esphome/components/sensor/sensor.h>
#include <esphome/components/i2c/i2c.h>
#include <Arduino.h>

namespace esphome {
namespace vl53l1x {

static const char *const TAG = "vl53l1x";
using VL53L1X_Error = int8_t;
const uint8_t VL51L1X_DEFAULT_CONFIGURATION[] =
    {
        0x00, /* 0x2d : set bit 2 and 5 to 1 for fast plus mode (1MHz I2C), else don't touch */
        0x00, /* 0x2e : bit 0 if I2C pulled up at 1.8V, else set bit 0 to 1 (pull up at AVDD) */
        0x00, /* 0x2f : bit 0 if GPIO pulled up at 1.8V, else set bit 0 to 1 (pull up at AVDD) */
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
        0xcc, /* 0x5f : not user-modifiable */
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
                 vl53l1x_init() call, put 0x40 in location 0x87 */
};

class VL53L1XSensor :  // NOLINT(cppcoreguidelines-virtual-class-destructor)
                       public sensor::Sensor,
                       public PollingComponent,
                       public i2c::I2CDevice {
 public:
  uint16_t VL53L1X_I2C_SENSOR_DEVICE_ADDRESS = 0x0001;
  uint16_t VL53L1X_VHV_CONFIG_TIMEOUT_MACROP_LOOP_BOUND = 0x0008;
  uint16_t ALGO_CROSSTALK_COMPENSATION_PLANE_OFFSET_KCPS = 0x0016;
  uint16_t ALGO_CROSSTALK_COMPENSATION_X_PLANE_GRADIENT_KCPS = 0x0018;
  uint16_t ALGO_CROSSTALK_COMPENSATION_Y_PLANE_GRADIENT_KCPS = 0x001A;
  uint16_t ALGO_PART_TO_PART_RANGE_OFFSET_MM = 0x001E;
  uint16_t MM_CONFIG_INNER_OFFSET_MM = 0x0020;
  uint16_t MM_CONFIG_OUTER_OFFSET_MM = 0x0022;
  uint16_t PAD_I2C_HV_EXTSUP_CONFIG = 0x002E;
  uint16_t GPIO_HV_MUX_CTRL = 0x0030;
  uint16_t GPIO_TIO_HV_STATUS = 0x0031;
  uint16_t SYSTEM_INTERRUPT_CONFIG_GPIO = 0x0046;
  uint16_t PHASECAL_CONFIG_TIMEOUT_MACROP = 0x004B;
  uint16_t RANGE_CONFIG_TIMEOUT_MACROP_A_HI = 0x005E;
  uint16_t RANGE_CONFIG_VCSEL_PERIOD_A = 0x0060;
  uint16_t RANGE_CONFIG_VCSEL_PERIOD_B = 0x0063;
  uint16_t RANGE_CONFIG_TIMEOUT_MACROP_B_HI = 0x0061;
  uint16_t RANGE_CONFIG_TIMEOUT_MACROP_B_LO = 0x0062;
  uint16_t RANGE_CONFIG_SIGMA_THRESH = 0x0064;
  uint16_t RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT_MCPS = 0x0066;
  uint16_t RANGE_CONFIG_VALID_PHASE_HIGH = 0x0069;
  uint16_t VL53L1X_SYSTEM_INTERMEASUREMENT_PERIOD = 0x006C;
  uint16_t SYSTEM_THRESH_HIGH = 0x0072;
  uint16_t SYSTEM_THRESH_LOW = 0x0074;
  uint16_t SD_CONFIG_WOI_SD0 = 0x0078;
  uint16_t SD_CONFIG_INITIAL_PHASE_SD0 = 0x007A;
  uint16_t ROI_CONFIG_USER_ROI_CENTRE_SPAD = 0x007F;
  uint16_t ROI_CONFIG_USER_ROI_REQUESTED_GLOBAL_XY_SIZE = 0x0080;
  uint16_t SYSTEM_SEQUENCE_CONFIG = 0x0081;
  uint16_t VL53L1X_SYSTEM_GROUPED_PARAMETER_HOLD = 0x0082;
  uint16_t SYSTEM_INTERRUPT_CLEAR = 0x0086;
  uint16_t SYSTEM_MODE_START = 0x0087;
  uint16_t VL53L1X_RESULT_RANGE_STATUS = 0x0089;
  uint16_t VL53L1X_RESULT_DSS_ACTUAL_EFFECTIVE_SPADS_SD0 = 0x008C;
  uint16_t RESULT_AMBIENT_COUNT_RATE_MCPS_SD = 0x0090;
  uint16_t VL53L1X_RESULT_FINAL_CROSSTALK_CORRECTED_RANGE_MM_SD0 = 0x0096;
  uint16_t VL53L1X_RESULT_PEAK_SIGNAL_COUNT_RATE_CROSSTALK_CORRECTED_MCPS_SD0 = 0x0098;
  uint16_t VL53L1X_RESULT_OSC_CALIBRATE_VAL = 0x00DE;
  uint16_t VL53L1X_FIRMWARE_SYSTEM_STATUS = 0x00E5;
  uint16_t VL53L1X_IDENTIFICATION_MODEL_ID = 0x010F;
  uint16_t VL53L1X_ROI_CONFIG_MODE_ROI_CENTRE_SPAD = 0x013E;

  VL53L1X_Error VL53L1X_ERROR_NONE = 0;
  VL53L1X_Error VL53L1X_ERROR_CALIBRATION_WARNING = -1;
  /**!< Warning invalid calibration data may be in used
   *     \a vl53l1x_InitData()
   *     \a vl53l1x_GetOffsetCalibrationData
   *     \a vl53l1x_SetOffsetCalibrationData */
  VL53L1X_Error VL53L1X_ERROR_MIN_CLIPPED = -2;
  /*!< Warning parameter passed was clipped to min before to be applied */
  VL53L1X_Error VL53L1X_ERROR_UNDEFINED = -3;
  /*!< Unqualified error */
  VL53L1X_Error VL53L1X_ERROR_INVALID_PARAMS = -4;
  /*!< Parameter passed is invalid or out of range */
  VL53L1X_Error VL53L1X_ERROR_NOT_SUPPORTED = -5;
  /*!< Function is not supported in current mode or configuration */
  VL53L1X_Error VL53L1X_ERROR_RANGE_ERROR = -6;
  /*!< Device report a ranging error interrupt status */
  VL53L1X_Error VL53L1X_ERROR_TIME_OUT = -7;
  /*!< Aborted due to time out */
  VL53L1X_Error VL53L1X_ERROR_MODE_NOT_SUPPORTED = -8;
  /*!< Asked mode is not supported by the device */
  VL53L1X_Error VL53L1X_ERROR_BUFFER_TOO_SMALL = -9;
  /*!< ... */
  VL53L1X_Error VL53L1X_ERROR_COMMS_BUFFER_TOO_SMALL = -10;
  /*!< Supplied buffer is larger than I2C supports */
  VL53L1X_Error VL53L1X_ERROR_GPIO_NOT_EXISTING = -11;
  /*!< User tried to setup a non-existing GPIO pin */
  VL53L1X_Error VL53L1X_ERROR_GPIO_FUNCTIONALITY_NOT_SUPPORTED = -12;
  /*!< unsupported GPIO functionality */
  VL53L1X_Error VL53L1X_ERROR_CONTROL_INTERFACE = -13;
  /*!< error reported from IO functions */
  VL53L1X_Error VL53L1X_ERROR_INVALID_COMMAND = -14;
  /*!< The command is not allowed in the current device state (power down) */
  VL53L1X_Error VL53L1X_ERROR_DIVISION_BY_ZERO = -15;
  /*!< In the function a division by zero occurs */
  VL53L1X_Error VL53L1X_ERROR_REF_SPAD_INIT = -16;
  /*!< Error during reference SPAD initialization */
  VL53L1X_Error VL53L1X_ERROR_GPH_SYNC_CHECK_FAIL = -17;
  /*!< GPH sync interrupt check fail - API out of sync with device*/
  VL53L1X_Error VL53L1X_ERROR_STREAM_COUNT_CHECK_FAIL = -18;
  /*!< Stream count check fail - API out of sync with device */
  VL53L1X_Error VL53L1X_ERROR_GPH_ID_CHECK_FAIL = -19;
  /*!< GPH ID check fail - API out of sync with device */
  VL53L1X_Error VL53L1X_ERROR_ZONE_STREAM_COUNT_CHECK_FAIL = -20;
  /*!< Zone dynamic config stream count check failed - API out of sync */
  VL53L1X_Error VL53L1X_ERROR_ZONE_GPH_ID_CHECK_FAIL = -21;
  /*!< Zone dynamic config GPH ID check failed - API out of sync */
  VL53L1X_Error VL53L1X_ERROR_XTALK_EXTRACTION_NO_SAMPLE_FAIL = -22;
  /*!< Thrown when run_xtalk_extraction fn has 0 succesful samples
   *    when using the full array to sample the xtalk. In this case there is
   *    not enough information to generate new Xtalk parm info. The function
   *    will exit and leave the current xtalk parameters unaltered */
  VL53L1X_Error VL53L1X_ERROR_XTALK_EXTRACTION_SIGMA_LIMIT_FAIL = -23;
  /*!< Thrown when run_xtalk_extraction fn has found that the
   *    avg sigma estimate of the full array xtalk sample is > than the
   *    maximal limit allowed. In this case the xtalk sample is too noisy for
   *    measurement. The function will exit and leave the current xtalk parameters
   *    unaltered. */
  VL53L1X_Error VL53L1X_ERROR_OFFSET_CAL_NO_SAMPLE_FAIL = -24;
  /*!< Thrown if there one of stages has no valid offset calibration
   *    samples. A fatal error calibration not valid */
  VL53L1X_Error VL53L1X_ERROR_OFFSET_CAL_NO_SPADS_ENABLED_FAIL = -25;
  /*!< Thrown if there one of stages has zero effective SPADS
   *    Traps the case when MM1 SPADs is zero.
   *    A fatal error calibration not valid */
  VL53L1X_Error VL53L1X_ERROR_ZONE_CAL_NO_SAMPLE_FAIL = -26;
  /*!< Thrown if then some of the zones have no valid samples
   *    A fatal error calibration not valid */
  VL53L1X_Error VL53L1X_ERROR_TUNING_PARM_KEY_MISMATCH = -27;
  /*!< Thrown if the tuning file key table version does not match with
   *    expected value. The driver expects the key table version to match
   *    the compiled default version number in the define
   *    #vl53l1x_TUNINGPARM_KEY_TABLE_VERSION_DEFAULT */
  VL53L1X_Error VL53L1X_WARNING_REF_SPAD_CHAR_NOT_ENOUGH_SPADS = -28;
  /*!< Thrown if there are less than 5 good SPADs are available. */
  VL53L1X_Error VL53L1X_WARNING_REF_SPAD_CHAR_RATE_TOO_HIGH = -29;
  /*!< Thrown if the final reference rate is greater than
   *    the upper reference rate limit - default is 40 Mcps.
   *    Implies a minimum Q3 (x10) SPAD (5) selected */
  VL53L1X_Error VL53L1X_WARNING_REF_SPAD_CHAR_RATE_TOO_LOW = -30;
  /*!< Thrown if the final reference rate is less than
   *    the lower reference rate limit - default is 10 Mcps.
   *    Implies maximum Q1 (x1) SPADs selected */
  VL53L1X_Error VL53L1X_WARNING_OFFSET_CAL_MISSING_SAMPLES = -31;
  /*!< Thrown if there is less than the requested number of
   *    valid samples. */
  VL53L1X_Error VL53L1X_WARNING_OFFSET_CAL_SIGMA_TOO_HIGH = -32;
  /*!< Thrown if the offset calibration range sigma estimate is greater
   *    than 8.0 mm. This is the recommended min value to yield a stable
   *    offset measurement */
  VL53L1X_Error VL53L1X_WARNING_OFFSET_CAL_RATE_TOO_HIGH = -33;
  /*!< Thrown when vl53l1x_run_offset_calibration()  peak rate is greater
   *    than that 50.0Mcps. This is the recommended  max rate to avoid
   *    pile-up influencing the offset measurement */
  VL53L1X_Error VL53L1X_WARNING_OFFSET_CAL_SPAD_COUNT_TOO_LOW = -34;
  /*!< Thrown when vl53l1x_run_offset_calibration() when one of stages
   *    range has less that 5.0 effective SPADS. This is the recommended
   *    min value to yield a stable offset */
  VL53L1X_Error VL53L1X_WARNING_ZONE_CAL_MISSING_SAMPLES = -35;
  /*!< Thrown if one of more of the zones have less than
   *    the requested number of valid samples */
  VL53L1X_Error VL53L1X_WARNING_ZONE_CAL_SIGMA_TOO_HIGH = -36;
  /*!< Thrown if one or more zones have sigma estimate value greater
   *    than 8.0 mm. This is the recommended min value to yield a stable
   *    offset measurement */
  VL53L1X_Error VL53L1X_WARNING_ZONE_CAL_RATE_TOO_HIGH = -37;
  /*!< Thrown if one of more zones have  peak rate higher than
   *    that 50.0Mcps. This is the recommended  max rate to avoid
   *    pile-up influencing the offset measurement */
  VL53L1X_Error VL53L1X_WARNING_XTALK_MISSING_SAMPLES = -38;
  /*!< Thrown to notify that some of the xtalk samples did not yield
   *    valid ranging pulse data while attempting to measure
   *    the xtalk signal in vl53l1_run_xtalk_extract(). This can signify any of
   *    the zones are missing samples, for further debug information the
   *    xtalk_results struct should be referred to. This warning is for
   *    notification only, the xtalk pulse and shape have still been generated. */
  VL53L1X_Error VL53L1X_WARNING_XTALK_NO_SAMPLES_FOR_GRADIENT = -39;
  /*!< Thrown to notify that some of teh xtalk samples used for gradient
   *    generation did not yield valid ranging pulse data while attempting to
   *    measure the xtalk signal in vl53l1_run_xtalk_extract(). This can signify
   *    that any one of the zones 0-3 yielded no successful samples. The
   *    xtalk_results struct should be referred to for further debug info.
   *    This warning is for notification only, the xtalk pulse and shape
   *    have still been generated. */
  VL53L1X_Error VL53L1X_WARNING_XTALK_SIGMA_LIMIT_FOR_GRADIENT = -40;
  /*!< Thrown to notify that some of the xtalk samples used for gradient
   *    generation did not pass the sigma limit check  while attempting to
   *    measure the xtalk signal in vl53l1_run_xtalk_extract(). This can signify
   *    that any one of the zones 0-3 yielded an avg sigma_mm value > the limit.
   *    The xtalk_results struct should be referred to for further debug info.
   *    This warning is for notification only, the xtalk pulse and shape
   *    have still been generated. */
  VL53L1X_Error VL53L1X_ERROR_NOT_IMPLEMENTED = -41;
  /*!< Tells requested functionality has not been implemented yet or
   *    not compatible with the device */
  VL53L1X_Error VL53L1X_ERROR_PLATFORM_SPECIFIC_START = -60;
  /*!< Tells the starting code for platform */

  VL53L1XSensor();
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void update() override;
  //  void loop() override;

  static std::list<VL53L1XSensor *> vl53l1x_sensors;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
  VL53L1X_Error vl_status;                            /**< VL53L1X API Error Status */
  const uint8_t VL53L1X_I2C_ADDR = 0x29;              ///< Default sensor I2C address

  bool begin(uint8_t i2c_addr);
  uint16_t sensor_id();
  bool start_ranging();
  bool stop_ranging();
  bool data_ready();
  int16_t distance();
  bool clear_interrupt();
  bool set_int_polarity(bool polarity);
  bool get_int_polarity();

  /*** Interface Methods ***/
  /*** High level API ***/
  /**
   * @brief       PowerOn the sensor
   * @return      void
   */
  /* turns on the sensor */
  virtual void vl53l1x_on() {
    if (this->irq_pin_ != nullptr) {
      this->irq_pin_->digital_write(HIGH);
    }
    delay(10);
  }

  /**
   * @brief       PowerOff the sensor
   * @return      void
   */
  /* turns off the sensor */
  virtual void vl53l1x_off() {
    if (this->irq_pin_ != nullptr) {
      this->irq_pin_->digital_write(LOW);
    }
    delay(10);
  }

  /**
   * @brief       Initialize the sensor with default values
   * @return      0 on Success
   */
  VL53L1X_Error init_sensor(uint8_t address) {
    VL53L1X_Error status = 0;
    uint8_t sensor_state = 0;
    this->vl53l1x_off();
    this->vl53l1x_on();

    if (address != this->VL53L1X_I2C_ADDR) {
      status = this->vl53l1x_set_i2c_address(address);
    }

    while (!sensor_state && !status) {
      status = this->vl53l1x_boot_state(&sensor_state);
      delay(2);
    }
    if (!status) {
      status = this->vl53l1x_sensor_init();
    }
    return status;
  }

  /* vl53l1x_api.h functions */

  /**
   * @brief This function sets the sensor I2C address used in case multiple devices application, default address 0x52
   */
  VL53L1X_Error vl53l1x_set_i2c_address(uint8_t new_address);

  /**
   * @brief This function loads the 135 bytes default values to initialize the sensor.
   * @param dev Device address
   * @return 0:success, != 0:failed
   */
  VL53L1X_Error vl53l1x_sensor_init();

  /**
   * @brief This function clears the interrupt, to be called after a ranging data reading
   * to arm the interrupt for the next data ready event.
   */
  VL53L1X_Error vl53l1x_clear_interrupt();

  /**
   * @brief This function programs the interrupt polarity\n
   * 1=active high (default), 0=active low
   */
  VL53L1X_Error vl53l1x_set_interrupt_polarity(uint8_t int_pol);

  /**
   * @brief This function returns the current interrupt polarity\n
   * 1=active high (default), 0=active low
   */
  VL53L1X_Error vl53l1x_get_interrupt_polarity(uint8_t *p_int_pol);

  /**
   * @brief This function starts the ranging distance operation\n
   * The ranging operation is continuous. The clear interrupt has to be done after each get data to allow the interrupt
   * to raise when the next data is ready\n 1=active high (default), 0=active low, use SetInterruptPolarity() to change
   * the interrupt polarity if required.
   */
  VL53L1X_Error vl53l1x_start_ranging();

  /**
   * @brief This function stops the ranging.
   */
  VL53L1X_Error vl53l1x_stop_ranging();

  /**
   * @brief This function checks if the new ranging data is available by polling the dedicated register.
   * @param : isDataReady==0 -> not ready; isDataReady==1 -> ready
   */
  VL53L1X_Error vl53l1x_check_for_data_ready(uint8_t *is_data_ready);

  /**
   * @brief This function programs the timing budget in ms.
   * Predefined values = 15, 20, 33, 50, 100(default), 200, 500.
   */
  VL53L1X_Error vl53l1x_set_timing_budget_in_ms(uint16_t timing_budget_in_ms);

  /**
   * @brief This function returns the current timing budget in ms.
   */
  VL53L1X_Error vl53l1x_get_timing_budget_in_ms(uint16_t *p_timing_budget_in_ms);

  /**
   * @brief This function programs the distance mode (1=short, 2=long(default)).
   * Short mode max distance is limited to 1.3 m but better ambient immunity.\n
   * Long mode can range up to 4 m in the dark with 200 ms timing budget.
   */
  VL53L1X_Error vl53l1x_set_distance_mode(uint16_t dist_mode);

  /**
   * @brief This function returns the current distance mode (1=short, 2=long).
   */
  VL53L1X_Error vl53l1x_get_distance_mode(uint16_t *dist_mode);

  /**
   * @brief This function programs the Intermeasurement period in ms\n
   * Intermeasurement period must be >/= timing budget. This condition is not checked by the API,
   * the customer has the duty to check the condition. Default = 100 ms
   */
  VL53L1X_Error vl53l1x_set_inter_measurement_in_ms(uint16_t inter_measurement_in_ms);

  /**
   * @brief This function returns the Intermeasurement period in ms.
   */
  VL53L1X_Error vl53l1x_get_inter_measurement_in_ms(uint16_t *p_im);

  /**
   * @brief This function returns the boot state of the device (1:booted, 0:not booted)
   */
  VL53L1X_Error vl53l1x_boot_state(uint8_t *state);

  /**
   * @brief This function returns the sensor id, sensor Id must be 0xEEAC
   */
  VL53L1X_Error vl53l1x_get_sensor_id(uint16_t *sensor_id);

  /**
   * @brief This function returns the distance measured by the sensor in mm
   */
  VL53L1X_Error vl53l1x_get_distance(uint16_t *distance);

  /**
   * @brief This function returns the returned signal per SPAD in kcps/SPAD.
   * With kcps stands for Kilo Count Per Second
   */
  VL53L1X_Error vl53l1x_get_signal_per_spad(uint16_t *signal_per_sp);

  /**
   * @brief This function returns the ambient per SPAD in kcps/SPAD
   */
  VL53L1X_Error vl53l1x_get_ambient_per_spad(uint16_t *amb);

  /**
   * @brief This function returns the returned signal in kcps.
   */
  VL53L1X_Error vl53l1x_get_signal_rate(uint16_t *signal_rate);

  /**
   * @brief This function returns the current number of enabled SPADs
   */
  VL53L1X_Error vl53l1x_get_spad_nb(uint16_t *sp_nb);

  /**
   * @brief This function returns the ambient rate in kcps
   */
  VL53L1X_Error vl53l1x_get_ambient_rate(uint16_t *amb_rate);

  /**
   * @brief This function returns the ranging status error \n
   * (0:no error, 1:sigma failed, 2:signal failed, ..., 7:wrap-around)
   */
  VL53L1X_Error vl53l1x_get_range_status(uint8_t *range_status);

  /**
   * @brief This function programs the offset correction in mm
   * @param OffsetValue:the offset correction value to program in mm
   */
  VL53L1X_Error vl53l1x_set_offset(int16_t offset_value);

  /**
   * @brief This function returns the programmed offset correction value in mm
   */
  VL53L1X_Error vl53l1x_get_offset(int16_t *offset);

  /**
   * @brief This function programs the xtalk correction value in cps (Count Per Second).\n
   * This is the number of photons reflected back from the cover glass in cps.
   */
  VL53L1X_Error vl53l1x_set_x_talk(uint16_t xtalk_value);

  /**
   * @brief This function returns the current programmed xtalk correction value in cps
   */
  VL53L1X_Error vl53l1x_get_x_talk(uint16_t *xtalk);

  /**
   * @brief This function programs the threshold detection mode\n
   * Example:\n
   * vl53l1x_SetDistanceThreshold(dev,100,300,0,1): Below 100 \n
   * vl53l1x_SetDistanceThreshold(dev,100,300,1,1): Above 300 \n
   * vl53l1x_SetDistanceThreshold(dev,100,300,2,1): Out of window \n
   * vl53l1x_SetDistanceThreshold(dev,100,300,3,1): In window \n
   * @param dev : device address
   * @param ThreshLow(in mm) : the threshold under which one the device raises an interrupt if Window = 0
   * @param ThreshHigh(in mm) :  the threshold above which one the device raises an interrupt if Window = 1
   * @param Window detection mode : 0=below, 1=above, 2=out, 3=in
   * @param IntOnNoTarget = 1 (No longer used - just use 1)
   */
  VL53L1X_Error vl53l1x_set_distance_threshold(uint16_t thresh_low, uint16_t thresh_high, uint8_t window,
                                               uint8_t int_on_no_target);

  /**
   * @brief This function returns the window detection mode (0=below; 1=above; 2=out; 3=in)
   */
  VL53L1X_Error vl53l1x_get_distance_threshold_window(uint16_t *window);

  /**
   * @brief This function returns the low threshold in mm
   */
  VL53L1X_Error vl53l1x_get_distance_threshold_low(uint16_t *low);

  /**
   * @brief This function returns the high threshold in mm
   */
  VL53L1X_Error vl53l1x_get_distance_threshold_high(uint16_t *high);

  /**
   * @brief This function programs the ROI (Region of Interest)\n
   * The ROI position is centered, only the ROI size can be reprogrammed.\n
   * The smallest acceptable ROI size = 4\n
   * @param roi_x:ROI Width; roi_y=ROI Height
   */
  VL53L1X_Error vl53l1x_set_roi(uint16_t roi_x, uint16_t roi_y);

  /**
   *@brief This function returns width X and height Y
   */
  VL53L1X_Error vl53l1x_get_roi_xy(uint16_t *roi_x, uint16_t *roi_y);

  /**
   *@brief This function programs the new user ROI center, please to be aware that there is no check in this function.
   *if the ROI center vs ROI size is out of border the ranging function return error #13
   */
  VL53L1X_Error vl53l1x_set_roi_center(uint8_t roi_center);

  /**
   *@brief This function returns the current user ROI center
   */
  VL53L1X_Error vl53l1x_get_roi_center(uint8_t *roi_center);

  /**
   * @brief This function programs a new signal threshold in kcps (default=1024 kcps\n
   */
  VL53L1X_Error vl53l1x_set_signal_threshold(uint16_t signal);

  /**
   * @brief This function returns the current signal threshold in kcps
   */
  VL53L1X_Error vl53l1x_get_signal_threshold(uint16_t *signal);

  /**
   * @brief This function programs a new sigma threshold in mm (default=15 mm)
   */
  VL53L1X_Error vl53l1x_set_sigma_threshold(uint16_t sigma);

  /**
   * @brief This function returns the current sigma threshold in mm
   */
  VL53L1X_Error vl53l1x_get_sigma_threshold(uint16_t *signal);

  /**
   * @brief This function performs the temperature calibration.
   * It is recommended to call this function any time the temperature might have changed by more than 8 deg C
   * without sensor ranging activity for an extended period.
   */
  VL53L1X_Error vl53l1x_start_temperature_update();

  /* vl53l1x_calibration.h functions */

  /**
   * @brief This function performs the offset calibration.\n
   * The function returns the offset value found and programs the offset compensation into the device.
   * @param TargetDistInMm target distance in mm, ST recommended 100 mm
   * Target reflectance = grey17%
   * @return 0:success, !=0: failed
   * @return offset pointer contains the offset found in mm
   */
  int8_t vl53l1x_calibrate_offset(uint16_t target_dist_in_mm, int16_t *offset);

  /**
   * @brief This function performs the xtalk calibration.\n
   * The function returns the xtalk value found and programs the xtalk compensation to the device
   * @param TargetDistInMm target distance in mm\n
   * The target distance : the distance where the sensor start to "under range"\n
   * due to the influence of the photons reflected back from the cover glass becoming strong\n
   * It's also called inflection point\n
   * Target reflectance = grey 17%
   * @return 0: success, !=0: failed
   * @return xtalk pointer contains the xtalk value found in cps (number of photons in count per second)
   */
  int8_t vl53l1x_calibrate_x_talk(uint16_t target_dist_in_mm, uint16_t *xtalk);

  /* Write and read functions from I2C */
  VL53L1X_Error vl53l1x_wr_byte(uint16_t index, uint8_t data);
  VL53L1X_Error vl53l1x_wr_word(uint16_t index, uint16_t data);
  VL53L1X_Error vl53l1x_wr_dword(uint16_t index, uint32_t data);
  VL53L1X_Error vl53l1x_rd_byte(uint16_t index, uint8_t *data);
  VL53L1X_Error vl53l1x_rd_word(uint16_t index, uint16_t *data);
  VL53L1X_Error vl53l1x_rd_dword(uint16_t index, uint32_t *data);
  VL53L1X_Error vl53l1x_update_byte(uint16_t index, uint8_t and_data, uint8_t or_data);
  VL53L1X_Error vl53l1x_write_multi(uint16_t index, uint8_t *pdata, uint32_t count);
  VL53L1X_Error vl53l1x_read_multi(uint16_t index, uint8_t *pdata, uint32_t count);
  VL53L1X_Error vl53l1x_i2c_write(uint16_t index, const uint8_t *data, uint16_t number_of_bytes);
  VL53L1X_Error vl53l1x_i2c_read(uint16_t index, uint8_t *data, uint16_t number_of_bytes);
  VL53L1X_Error vl53l1x_get_tick_count(uint32_t *ptick_count_ms);
  VL53L1X_Error vl53l1x_wait_us(int32_t wait_us);
  VL53L1X_Error vl53l1x_wait_ms(int32_t wait_ms);
  VL53L1X_Error vl53l1x_wait_value_mask_ex(uint32_t timeout_ms, uint16_t index, uint8_t value, uint8_t mask,
                                           uint32_t poll_delay_ms);

  // esphome configuration setters
  void set_enable_pin(GPIOPin *enable) { this->enable_pin_ = enable; }
  void set_irg_pin(GPIOPin *irq) { this->irq_pin_ = irq; }
  void set_io_2v8(bool io_2v8) { this->io_2v8_ = io_2v8; }
  void set_long_range(bool long_range) { this->long_range_ = long_range; }
  void set_timing_budget(uint16_t timing_budget) { this->timing_budget_ = timing_budget; }
  void set_offset(int16_t offset) { this->offset_ = offset; }

 protected:
  static bool enable_pin_setup_complete;           // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

  /* esphome configuration variables */
  GPIOPin *enable_pin_{nullptr};
  GPIOPin *irq_pin_{nullptr};
  bool io_2v8_ = false;
  bool long_range_ = true;
  uint16_t timing_budget_ = 100;
  int16_t offset_ = 0;
};

}  // namespace vl53l1x
}  // namespace esphome
