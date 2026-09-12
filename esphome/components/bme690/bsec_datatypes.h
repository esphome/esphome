#pragma once

// C ABI header; BME690 C++ wrapper code lives in namespace esphome::bme690.

/**
 * Copyright (C) Bosch Sensortec GmbH. All Rights Reserved. Confidential.
 *
 * Disclaimer
 *
 * Common:
 * Bosch Sensortec products are developed for the consumer goods industry. They may only be used
 * within the parameters of the respective valid product data sheet. Bosch Sensortec products are
 * provided with the express understanding that there is no warranty of fitness for a particular purpose.
 * They are not fit for use in life-sustaining, safety or security sensitive systems or any system or device
 * that may lead to bodily harm or property damage if the system or device malfunctions. In addition,
 * Bosch Sensortec products are not fit for use in products which interact with motor vehicle systems.
 * The resale and/or use of products are at the purchaser's own risk and his own responsibility. The
 * examination of fitness for the intended use is the sole responsibility of the Purchaser.
 *
 * The purchaser shall indemnify Bosch Sensortec from all third party claims, including any claims for
 * incidental, or consequential damages, arising from any product use not covered by the parameters of
 * the respective valid product data sheet or not approved by Bosch Sensortec and reimburse Bosch
 * Sensortec for all costs in connection with such claims.
 *
 * The purchaser must monitor the market for the purchased products, particularly with regard to
 * product safety and inform Bosch Sensortec without delay of all security relevant incidents.
 *
 * Engineering Samples are marked with an asterisk (*) or (e). Samples may vary from the valid
 * technical specifications of the product series. They are therefore not intended or fit for resale to third
 * parties or for use in end products. Their sole purpose is internal client testing. The testing of an
 * engineering sample may in no way replace the testing of a product series. Bosch Sensortec
 * assumes no liability for the use of engineering samples. By accepting the engineering samples, the
 * Purchaser agrees to indemnify Bosch Sensortec from all claims arising from the use of engineering
 * samples.
 *
 * Special:
 * This software module (hereinafter called "Software") and any information on application-sheets
 * (hereinafter called "Information") is provided free of charge for the sole purpose to support your
 * application work. The Software and Information is subject to the following terms and conditions:
 *
 * The Software is specifically designed for the exclusive use for Bosch Sensortec products by
 * personnel who have special experience and training. Do not use this Software if you do not have the
 * proper experience or training.
 *
 * This Software package is provided `` as is `` and without any expressed or implied warranties,
 * including without limitation, the implied warranties of merchantability and fitness for a particular
 * purpose.
 *
 * Bosch Sensortec and their representatives and agents deny any liability for the functional impairment
 * of this Software in terms of fitness, performance and safety. Bosch Sensortec and their
 * representatives and agents shall not be liable for any direct or indirect damages or injury, except as
 * otherwise stipulated in mandatory applicable law.
 *
 * The Information provided is believed to be accurate and reliable. Bosch Sensortec assumes no
 * responsibility for the consequences of use of such Information nor for any infringement of patents or
 * other rights of third parties which may result from its use. No license is granted by implication or
 * otherwise under any patent or patent rights of Bosch. Specifications mentioned in the Information are
 * subject to change without notice.
 *
 * It is not allowed to deliver the source code of the Software to any third party without permission of
 * Bosch Sensortec.
 *
 * @file        bsec_datatypes.h
 *
 * @brief
 * Contains the data types used by BSEC
 *
 */

#ifndef BSEC_DATATYPES_H_
#define BSEC_DATATYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @addtogroup bsec_datatypes BSEC Enumerations and Definitions
 * @brief
 * @{*/

#ifdef __KERNEL__
#include <linux/types.h>
#endif
#include <stdint.h>
#include <stddef.h>

#define BSEC_MAX_WORKBUFFER_SIZE (4096) /*!< Maximum size (in bytes) of the work buffer */
#define BSEC_MAX_PHYSICAL_SENSOR \
  (8) /*!< Number of physical sensors that need allocated space before calling bsec_update_subscription() */
#define BSEC_MAX_PROPERTY_BLOB_SIZE \
  (2005) /*!< Maximum size (in bytes) of the data blobs returned by bsec_get_configuration() */
#define BSEC_MAX_STATE_BLOB_SIZE (259) /*!< Maximum size (in bytes) of the data blobs returned by bsec_get_state()*/
#define BSEC_SAMPLE_RATE_DISABLED (65535.0f) /*!< Sample rate of a disabled sensor */
#define BSEC_SAMPLE_RATE_ULP (0.0033333f)    /*!< Sample rate in case of Ultra Low Power Mode */
#define BSEC_SAMPLE_RATE_CONT (1.0f)         /*!< Sample rate in case of Continuous Mode */
#define BSEC_SAMPLE_RATE_LP (0.33333f)       /*!< Sample rate in case of Low Power Mode */
#define BSEC_SAMPLE_RATE_ULP_MEASUREMENT_ON_DEMAND \
  (0.0f)                                  /*!< Input value used to trigger an extra measurment (ULP plus) */
#define BSEC_SAMPLE_RATE_SCAN (0.055556f) /*!< Sample rate in case of scan mode */

#define BSEC_PROCESS_PRESSURE \
  (1 << (BSEC_INPUT_PRESSURE - 1)) /*!< process_data bitfield constant for pressure @sa bsec_bme_settings_t */
#define BSEC_PROCESS_TEMPERATURE \
  (1 << (BSEC_INPUT_TEMPERATURE - 1)) /*!< process_data bitfield constant for temperature @sa bsec_bme_settings_t */
#define BSEC_PROCESS_HUMIDITY \
  (1 << (BSEC_INPUT_HUMIDITY - 1)) /*!< process_data bitfield constant for humidity @sa bsec_bme_settings_t */
#define BSEC_PROCESS_GAS \
  (1 << (BSEC_INPUT_GASRESISTOR - 1)) /*!< process_data bitfield constant for gas sensor @sa bsec_bme_settings_t */
#define BSEC_PROCESS_PROFILE_PART \
  (1 << (BSEC_INPUT_PROFILE_PART - 1))    /*!< process_data bitfield constant for gas sensor @sa bsec_bme_settings_t */
#define BSEC_NUMBER_OUTPUTS (24)          /*!< Number of outputs, depending on solution */
#define BSEC_OUTPUT_INCLUDED (2146597359) /*!< bitfield that indicates which outputs are included in the solution */

/*!
 * @brief Enumeration for input (physical) sensors.
 *
 * Used to populate bsec_input_t::sensor_id. It is also used in bsec_sensor_configuration_t::sensor_id structs
 * returned in the parameter required_sensor_settings of bsec_update_subscription().
 *
 * @sa bsec_sensor_configuration_t @sa bsec_input_t
 */
typedef enum {  // NOLINT(modernize-use-using)
  /**
   * @brief Pressure sensor output of BME6xy [Pa]
   */
  BSEC_INPUT_PRESSURE = 1,

  /**
   * @brief Humidity sensor output of BME6xy [%]
   *
   * @note Relative humidity strongly depends on the temperature (it is measured at). It may require a conversion to
   * the temperature outside of the device.
   *
   * @sa bsec_virtual_sensor_t
   */
  BSEC_INPUT_HUMIDITY = 2,

  /**
   * @brief Temperature sensor output of BME6xy [degrees Celsius]
   *
   * @note The BME6xy is factory trimmed, thus the temperature sensor of the BME6xy is very accurate.
   * The temperature value is a very local measurement value and can be influenced by external heat sources.
   *
   * @sa bsec_virtual_sensor_t
   */
  BSEC_INPUT_TEMPERATURE = 3,

  /**
   * @brief Gas sensor resistance output of BME6xy [Ohm]
   *
   * The resistance value changes due to varying VOC concentrations (the higher the concentration of reducing VOCs,
   * the lower the resistance and vice versa).
   */
  BSEC_INPUT_GASRESISTOR = 4, /*!<  */

  /**
   * @brief Additional input for device heat compensation
   *
   * The value is subtracted from ::BSEC_INPUT_TEMPERATURE to compute
   * ::BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE.
   *
   * @sa bsec_virtual_sensor_t
   */
  BSEC_INPUT_HEATSOURCE = 14,

  /**
   * @brief Additional input for controlling IAQ and TVOC equivalent baseline trackers.
   *
   * Allowed input values are:
   *
   * 0 - Default value. IAQ baseline adaptation is enabled and TVOC equivalent baseline adpatation is disabled.
   *
   * 1 - IAQ baseline adaptation is disabled and wait up to 10 seconds before restarting adaptation when input is reset
   *to 0.
   *
   * 2 - IAQ baseline adaptation is disabled and wait up to 3 seconds before restarting adaptation when input is reset
   *to 0.
   *
   * 3 - TVOC equivalent baseline adaptation enabled.
   *
   * @note Baseline is a mechanism for compensating the long term drifts in the enviornment and ensure the consistency
   *of IAQ and TVOC equivalent outputs across different BME sensors.
   */
  BSEC_INPUT_DISABLE_BASELINE_TRACKER = 23,

  /**
   * @brief Additional input that provides information about the state of the profile (1-9)
   *
   */
  BSEC_INPUT_PROFILE_PART = 24

} bsec_physical_sensor_t;

/*!
 * @brief Enumeration for output (virtual) sensors
 *
 * Used to populate bsec_output_t::sensor_id. It is also used in bsec_sensor_configuration_t::sensor_id structs
 * passed in the parameter requested_virtual_sensors of bsec_update_subscription().
 *
 * @sa bsec_sensor_configuration_t @sa bsec_output_t
 */
typedef enum {  // NOLINT(modernize-use-using)
  /**
   * @brief Indoor-air-quality estimate [0-500]
   *
   * Indoor-air-quality (IAQ) gives an indication of the relative change in ambient TVOCs detected by BME6xy.
   *
   * @note The IAQ scale ranges from 0 (clean air) to 500 (heavily polluted air). During operation, algorithms
   * automatically calibrate and adapt themselves to the typical environments where the sensor is operated
   * (e.g., home, workplace, inside a car, etc.).This automatic background calibration ensures that users experience
   * consistent IAQ performance. The calibration process considers the recent measurement history (typ. up to four
   * days) to ensure that IAQ=50 corresponds to typical good air and IAQ=200 indicates typical polluted air.
   */
  BSEC_OUTPUT_IAQ = 1,
  BSEC_OUTPUT_STATIC_IAQ = 2,            /*!< Unscaled indoor-air-quality estimate */
  BSEC_OUTPUT_CO2_EQUIVALENT = 3,        /*!< CO2 equivalent estimate [ppm] */
  BSEC_OUTPUT_BREATH_VOC_EQUIVALENT = 4, /*!< Breath VOC concentration estimate [ppm] */

  /**
   * @brief Temperature sensor signal [degrees Celsius]
   *
   * Temperature directly measured by BME6xy in degree Celsius.
   *
   * @note This value is cross-influenced by the sensor heating and device specific heating.
   */
  BSEC_OUTPUT_RAW_TEMPERATURE = 6,

  /**
   * @brief Pressure sensor signal [Pa]
   *
   * Pressure directly measured by the BME6xy in Pa.
   */
  BSEC_OUTPUT_RAW_PRESSURE = 7,

  /**
   * @brief Relative humidity sensor signal [%]
   *
   * Relative humidity directly measured by the BME6xy in %.
   *
   * @note This value is cross-influenced by the sensor heating and device specific heating.
   */
  BSEC_OUTPUT_RAW_HUMIDITY = 8,

  /**
   * @brief Gas sensor signal [Ohm]
   *
   * Gas resistance measured directly by the BME6xy in Ohm.The resistance value changes due to varying VOC
   * concentrations (the higher the concentration of reducing VOCs, the lower the resistance and vice versa).
   */
  BSEC_OUTPUT_RAW_GAS = 9,

  /**
   * @brief Gas sensor stabilization status [boolean]
   *
   * Indicates initial stabilization status of the gas sensor element: stabilization is ongoing (0) or stabilization
   * is finished (1).
   */
  BSEC_OUTPUT_STABILIZATION_STATUS = 12,

  /**
   * @brief Gas sensor run-in status [boolean]
   *
   * Dynamicaly tracks the power-on stabilization of the gas sensor element: stabilization is ongoing (0) or
   * stabilization is finished (1). It depends on how long the sensor has been turn off. A power-on conditioning has
   * been employed for early stabilization of BME sensors.
   */
  BSEC_OUTPUT_RUN_IN_STATUS = 13,

  /**
   * @brief Sensor heat compensated temperature [degrees Celsius]
   *
   * Temperature measured by BME6xy which is compensated for the influence of sensor (heater) in degree Celsius.
   * The self heating introduced by the heater is depending on the sensor operation mode and the sensor supply voltage.
   *
   *
   * @note In addition, the temperature output can be compensated by an user defined value
   * (::BSEC_INPUT_HEATSOURCE in degrees Celsius), which represents the device specific self-heating.
   *
   * Thus, the value is calculated as follows:
   * * ```BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE = ::BSEC_INPUT_TEMPERATURE -  function(sensor operation mode,
   * sensor supply voltage) - ::BSEC_INPUT_HEATSOURCE```
   *
   *
   * The self-heating in operation mode BSEC_SAMPLE_RATE_ULP is negligible.
   * The self-heating in operation mode BSEC_SAMPLE_RATE_LP is supported for 1.8V by default (no config file required).
   * If the BME6xy sensor supply voltage is 3.3V, the corresponding config file shall be used.
   */
  BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE = 14,

  /**
   * @brief Sensor heat compensated humidity [%]
   *
   * Relative humidity measured by BME6xy which is compensated for the influence of sensor (heater) in %.
   *
   * It converts the ::BSEC_INPUT_HUMIDITY from temperature ::BSEC_INPUT_TEMPERATURE to temperature
   * ::BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE.
   *
   * @note If ::BSEC_INPUT_HEATSOURCE is used for device specific temperature compensation, it will be
   * effective for ::BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY too.
   */
  BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY = 15,

  /**
   * @brief Raw gas resistance compensated by temperature and humidity influences [ohm].
   *
   * The effect of temperature and humidity on the gas resistance is eleminated to a good extend and it is further
   * linearized to log scale for estimation of the compensated gas resistance.
   *
   */
  BSEC_OUTPUT_COMPENSATED_GAS = 18,
  BSEC_OUTPUT_GAS_PERCENTAGE = 21, /*!< Percentage of min and max filtered gas value [%] */

  /**
   * @brief Gas estimate output channel 1 [0-1]
   *
   * The gas scan result is given in probability for each of the used gases.
   * In standard scan mode, the probability of H2S and non-H2S is provided by the variables BSEC_OUTPUT_GAS_ESTIMATE_1 &
   * BSEC_OUTPUT_GAS_ESTIMATE_2 respectively. A maximum of 4 classes can be used by configuring with BME AI-studio.
   */
  BSEC_OUTPUT_GAS_ESTIMATE_1 = 22,
  BSEC_OUTPUT_GAS_ESTIMATE_2 = 23, /*!< Gas estimate output channel 2 [0-1] */
  BSEC_OUTPUT_GAS_ESTIMATE_3 = 24, /*!< Gas estimate output channel 3 [0-1] */
  BSEC_OUTPUT_GAS_ESTIMATE_4 = 25, /*!< Gas estimate output channel 4 [0-1] */

  BSEC_OUTPUT_RAW_GAS_INDEX = 26, /*!< Gas index cyclically running from 0 to ::heater_profile_len-1, range of heater
                                     profile length is from 1 to 10, default being 10 */

  /**
   * @brief Regression estimate output channel 1
   *
   * The regression estimate gives the quantitative measurement of the gases used.
   * A maximum of 4 gas targets can be used by configuring with BME AI-studio.
   */
  BSEC_OUTPUT_REGRESSION_ESTIMATE_1 = 27,
  BSEC_OUTPUT_REGRESSION_ESTIMATE_2 = 28, /*!< Regression estimate output channel 2  */
  BSEC_OUTPUT_REGRESSION_ESTIMATE_3 = 29, /*!< Regression estimate output channel 3  */
  BSEC_OUTPUT_REGRESSION_ESTIMATE_4 = 30, /*!< Regression estimate output channel 4  */
  BSEC_OUTPUT_TVOC_EQUIVALENT = 31        /*!< TVOC Equivalent in ppb  */
} bsec_virtual_sensor_t;

/*!
 * @brief Enumeration for function return codes
 */
typedef enum {                      // NOLINT(modernize-use-using)
  BSEC_OK = 0,                      /*!< Function execution successful */
  BSEC_E_DOSTEPS_INVALIDINPUT = -1, /*!< Input (physical) sensor id passed to bsec_do_steps() is not in the valid range
                                       or not valid for requested virtual sensor */
  BSEC_E_DOSTEPS_VALUELIMITS =
      -2, /*!< Value of input (physical) sensor signal passed to bsec_do_steps() is not in the valid range */
  BSEC_W_DOSTEPS_TSINTRADIFFOUTOFRANGE = 4, /*!< Past timestamps passed to bsec_do_steps() */
  BSEC_E_DOSTEPS_DUPLICATEINPUT = -6, /*!< Duplicate input (physical) sensor ids passed as input to bsec_do_steps() */
  BSEC_I_DOSTEPS_NOOUTPUTSRETURNABLE =
      2, /*!< No memory allocated to hold return values from bsec_do_steps(), i.e., n_outputs == 0 */
  BSEC_W_DOSTEPS_EXCESSOUTPUTS = 3, /*!< Not enough memory allocated to hold return values from bsec_do_steps(), i.e.,
                                       n_outputs < maximum number of requested output (virtual) sensors */
  BSEC_W_DOSTEPS_GASINDEXMISS = 5,  /*!< Gas index not provided to bsec_do_steps() */
  BSEC_E_SU_WRONGDATARATE =
      -10, /*!< The sample_rate of the requested output (virtual) sensor passed to bsec_update_subscription() is zero */
  BSEC_E_SU_SAMPLERATELIMITS =
      -12, /*!< The sample_rate of the requested output (virtual) sensor passed to bsec_update_subscription() does not
              match with the sampling rate allowed for that sensor */
  BSEC_E_SU_DUPLICATEGATE =
      -13, /*!< Duplicate output (virtual) sensor ids requested through bsec_update_subscription() */
  BSEC_E_SU_INVALIDSAMPLERATE =
      -14, /*!< The sample_rate of the requested output (virtual) sensor passed to bsec_update_subscription() does not
              fall within the global minimum and maximum sampling rates  */
  BSEC_E_SU_GATECOUNTEXCEEDSARRAY =
      -15, /*!< Not enough memory allocated to hold returned input (physical) sensor data from
              bsec_update_subscription(), i.e., n_required_sensor_settings ::BSEC_MAX_PHYSICAL_SENSOR */
  BSEC_E_SU_SAMPLINTVLINTEGERMULT = -16, /*!< The sample_rate of the requested output (virtual) sensor passed to
                                            bsec_update_subscription() is not correct */
  BSEC_E_SU_MULTGASSAMPLINTVL = -17,     /*!< The sample_rate of the requested output (virtual), which requires the gas
                                            sensor, is not equal to the sample_rate that the gas sensor is being operated */
  BSEC_E_SU_HIGHHEATERONDURATION =
      -18, /*!< The duration of one measurement is longer than the requested sampling interval */
  BSEC_W_SU_UNKNOWNOUTPUTGATE =
      10, /*!< Output (virtual) sensor id passed to bsec_update_subscription() is not in the valid range; e.g.,
             n_requested_virtual_sensors > actual number of output (virtual) sensors requested */
  BSEC_W_SU_MODINNOULP = 11,
  /*!< ULP plus can not be requested in non-ulp mode */ /*MOD_ONLY*/
  BSEC_I_SU_SUBSCRIBEDOUTPUTGATES =
      12, /*!< No output (virtual) sensor data were requested via bsec_update_subscription() */
  BSEC_I_SU_GASESTIMATEPRECEDENCE =
      13,                            /*!< GAS_ESTIMATE is suscribed and take precedence over other requested outputs */
  BSEC_W_SU_SAMPLERATEMISMATCH = 14, /*!< Subscriped sample rate of the output is not matching with configured sample
                                        rate. For example if user used the configuration of ULP and outputs subscribed
                                        for LP mode this warning will inform the user about this mismatch*/
  BSEC_E_PARSE_SECTIONEXCEEDSWORKBUFFER =
      -32,                  /*!< n_work_buffer_size passed to bsec_set_[configuration/state]() not sufficient */
  BSEC_E_CONFIG_FAIL = -33, /*!< Configuration failed */
  BSEC_E_CONFIG_VERSIONMISMATCH = -34, /*!< Version encoded in serialized_[settings/state] passed to
                                          bsec_set_[configuration/state]() does not match with current version */
  BSEC_E_CONFIG_FEATUREMISMATCH =
      -35, /*!< Enabled features encoded in serialized_[settings/state] passed to bsec_set_[configuration/state]() does
              not match with current library implementation or subscribed outputs*/
  BSEC_E_CONFIG_CRCMISMATCH =
      -36, /*!< serialized_[settings/state] passed to bsec_set_[configuration/state]() is corrupted */
  BSEC_E_CONFIG_EMPTY =
      -37, /*!< n_serialized_[settings/state] passed to bsec_set_[configuration/state]() is to short to be valid */
  BSEC_E_CONFIG_INSUFFICIENTWORKBUFFER =
      -38, /*!< Provided work_buffer is not large enough to hold the desired string */
  BSEC_E_CONFIG_INVALIDSTRINGSIZE =
      -40, /*!< String size encoded in configuration/state strings passed to bsec_set_[configuration/state]() does not
              match with the actual string size n_serialized_[settings/state] passed to these functions */
  BSEC_E_CONFIG_INSUFFICIENTBUFFER = -41, /*!< String buffer insufficient to hold serialized data from BSEC library */
  BSEC_E_SET_INVALIDCHANNELIDENTIFIER =
      -100, /*!< Internal error code, size of work buffer in setConfig must be set to #BSEC_MAX_WORKBUFFER_SIZE */
  BSEC_E_SET_INVALIDLENGTH = -104,       /*!< Internal error code */
  BSEC_W_SC_CALL_TIMING_VIOLATION = 100, /*!< Difference between actual and defined sampling intervals of
                                            bsec_sensor_control() greater than allowed */
  BSEC_W_SC_MODEXCEEDULPTIMELIMIT = 101,
  /*!< ULP plus is not allowed because an ULP measurement just took or will take place */ /*MOD_ONLY*/
  BSEC_W_SC_MODINSUFFICIENTWAITTIME =
      102 /*!< ULP plus is not allowed because not sufficient time passed since last ULP plus */ /*MOD_ONLY*/
} bsec_library_return_t;

/*!
 * @brief Structure containing the version information
 *
 * Please note that configuration and state strings are coded to a specific version and will not be accepted by other
 * versions of BSEC.
 *
 */
typedef struct {        // NOLINT(modernize-use-using)
  uint8_t major;        /**< @brief Major version */
  uint8_t minor;        /**< @brief Minor version */
  uint8_t major_bugfix; /**< @brief Major bug fix version */
  uint8_t minor_bugfix; /**< @brief Minor bug fix version */
} bsec_version_t;

/*!
 * @brief Structure describing an input sample to the library
 *
 * Each input sample is provided to BSEC as an element in a struct array of this type. Timestamps must be provided
 * in nanosecond  resolution. Moreover, duplicate timestamps for subsequent samples are not allowed and will results in
 * an error code being returned from bsec_do_steps().
 *
 * The meaning unit of the signal field are determined by the bsec_input_t::sensor_id field content. Possible
 * bsec_input_t::sensor_id values and and their meaning are described in ::bsec_physical_sensor_t.
 *
 * @sa bsec_physical_sensor_t
 *
 */
typedef struct {  // NOLINT(modernize-use-using)
  /**
   * @brief Time stamp in nanosecond resolution [ns]
   *
   * Timestamps must be provided as non-repeating and increasing values. They can have their 0-points at system start or
   * at a defined wall-clock time (e.g., 01-Jan-1970 00:00:00)
   */
  int64_t time_stamp;
  float signal; /*!< @brief Signal sample in the unit defined for the respective sensor_id @sa bsec_physical_sensor_t */
  uint8_t signal_dimensions; /*!< @brief Signal dimensions (reserved for future use, shall be set to 1) */
  uint8_t sensor_id;         /*!< @brief Identifier of physical sensor @sa bsec_physical_sensor_t */
} bsec_input_t;

/*!
 * @brief Structure describing an output sample of the library
 *
 * Each output sample is returned from BSEC by populating the element of a struct array of this type. The contents of
 * the signal field is defined by the supplied bsec_output_t::sensor_id. Possible output
 * bsec_output_t::sensor_id values are defined in ::bsec_virtual_sensor_t.
 *
 * @sa bsec_virtual_sensor_t
 */
typedef struct {      // NOLINT(modernize-use-using)
  int64_t time_stamp; /*!< @brief Time stamp in nanosecond resolution as provided as input [ns] */
  float signal;       /*!< @brief Signal sample in the unit defined for the respective bsec_output_t::sensor_id @sa
                         bsec_virtual_sensor_t */
  uint8_t signal_dimensions; /*!< @brief Signal dimensions (reserved for future use, shall be set to 1) */
  uint8_t sensor_id;         /*!< @brief Identifier of virtual sensor @sa bsec_virtual_sensor_t  */

  /**
   * @brief Accuracy status 0-3
   *
   * Some virtual sensors provide a value in the accuracy field. If this is the case, the meaning of the field is as
   * follows:
   *
   * | Name                       | Value |  Accuracy description |
   * |----------------------------|-------|-------------------------------------------------------------------------------------------------------------|
   * | UNRELIABLE                 |   0   | Sensor data is unreliable, the sensor must be calibrated | | LOW_ACCURACY |
   * 1   | Reliability of virtual sensor is low, sensor should be calibrated | | MEDIUM_ACCURACY            |   2   |
   * Medium reliability, sensor calibration or training may improve performance | | HIGH_ACCURACY              |   3   |
   * High reliability                                                      |
   *
   * For example:
   *
   * - IAQ accuracy indicator will notify the user when she/he should initiate a calibration process. Calibration is
   *   performed automatically in the background if the sensor is exposed to clean and polluted air for approximately
   *   30 minutes each.
   *
   *   | Virtual sensor             | Value |  Accuracy description |
   *   |----------------------------|-------|---------------------------------------------------------------------------------------------------------------------------------------------------------------|
   *   | IAQ                        |   0   | Stabilization / run-in ongoing | |                            |   1   |
   * Low accuracy,to reach high accuracy(3),please expose sensor once to good air (e.g. outdoor air) and bad air (e.g.
   * box with exhaled breath) for auto-trimming  | |                            |   2   | Medium accuracy: auto-trimming
   * ongoing | |                            |   3   | High accuracy
   *
   * - Gas estimator accuracy indicator will notify the user when she/he gets valid gas prediction output.
   *
   *   | Virtual sensor             | Value |  Accuracy description |
   *   |----------------------------|-------|---------------------------------------------------------------------------------------------------------------------------------------------------------------|
   *   | GAS_ESTIMATE_x*            |   0   | No valid gas estimate found - BSEC collecting gas features | | |   3   |
   * Gas estimate prediction available |
   *
   *   <sup>*</sup> GAS_ESTIMATE_1, GAS_ESTIMATE_2, GAS_ESTIMATE_3, GAS_ESTIMATE_4
   * - Regression estimate accuracy  will notify the user when she/he gets valid regression estimate output.
   *
   *   | Virtual sensor             | Value |  Accuracy description |
   *   |----------------------------|-------|---------------------------------------------------------------------------------------------------------------------------------------------------------------|
   *   | REGRESSION_ESTIMATE_x*            |   0   | No valid regression estimate output found - BSEC collecting gas
   * features                                                                                                    | | |
   * 2   | Predicted regression estimate output is not within the value of the label present in the training data | | |
   * 3   | Predicted regression estimate output is within the the value of the label present in the training data |
   *
   *   <sup>*</sup> REGRESSION_ESTIMATE_1, REGRESSION_ESTIMATE_2, REGRESSION_ESTIMATE_3, REGRESSION_ESTIMATE_4
   */
  uint8_t accuracy;
} bsec_output_t;

/*!
 * @brief Structure describing sample rate of physical/virtual sensors
 *
 * This structure is used together with bsec_update_subscription() to enable BSEC outputs and to retrieve information
 * about the sample rates used for BSEC inputs.
 */
typedef struct {  // NOLINT(modernize-use-using)
  /**
   * @brief Sample rate of the virtual or physical sensor in Hertz [Hz]
   *
   * Only supported sample rates are allowed.
   */
  float sample_rate;

  /**
   * @brief Identifier of the virtual or physical sensor
   *
   * The meaning of this field changes depending on whether the structs are as the requested_virtual_sensors argument
   * to bsec_update_subscription() or as the required_sensor_settings argument.
   *
   * | bsec_update_subscription() argument | sensor_id field interpretation |
   * |-------------------------------------|--------------------------------|
   * | requested_virtual_sensors           | ::bsec_virtual_sensor_t        |
   * | required_sensor_settings            | ::bsec_physical_sensor_t       |
   *
   * @sa bsec_physical_sensor_t
   * @sa bsec_virtual_sensor_t
   */
  uint8_t sensor_id;
} bsec_sensor_configuration_t;

/*!
 * @brief Structure returned by bsec_sensor_control() to configure BME6xy sensor
 *
 * This structure contains settings that must be used to configure the BME6xy to perform a forced-mode measurement.
 * A measurement should only be executed if bsec_bme_settings_t::trigger_measurement is 1. If so, the oversampling
 * settings for temperature, humidity, and pressure should be set to the provided settings provided in
 * bsec_bme_settings_t::temperature_oversampling, bsec_bme_settings_t::humidity_oversampling, and
 * bsec_bme_settings_t::pressure_oversampling, respectively.
 *
 * In case of bsec_bme_settings_t::run_gas = 1, the gas sensor must be enabled with the provided
 * bsec_bme_settings_t::heater_temperature and bsec_bme_settings_t::heating_duration settings.
 */
typedef struct {               // NOLINT(modernize-use-using)
  int64_t next_call;           /*!< @brief Time stamp of the next call of the sensor_control*/
  uint32_t process_data;       /*!< @brief Bit field describing which data is to be passed to bsec_do_steps() @sa
                                  BSEC_PROCESS_GAS, BSEC_PROCESS_TEMPERATURE, BSEC_PROCESS_HUMIDITY, BSEC_PROCESS_PRESSURE */
  uint16_t heater_temperature; /*!< @brief Heater temperature [degrees Celsius] */
  uint16_t heater_duration;    /*!< @brief Heater duration [ms] */
  uint16_t heater_temperature_profile[10]; /*!< @brief Heater temperature profile [degrees Celsius] */
  uint16_t heater_duration_profile[10];    /*!< @brief Heater duration profile [ms] */
  uint8_t heater_profile_len;              /*!< @brief Heater profile length [1-10] */
  uint8_t run_gas;                         /*!< @brief Enable gas measurements [0/1] */
  uint8_t pressure_oversampling;           /*!< @brief Pressure oversampling settings [0-5] */
  uint8_t temperature_oversampling;        /*!< @brief Temperature oversampling settings [0-5] */
  uint8_t humidity_oversampling;           /*!< @brief Humidity oversampling settings [0-5] */
  uint8_t trigger_measurement;             /*!< @brief Trigger a forced measurement with these settings now [0/1] */
  uint8_t op_mode;                         /*!< @brief Sensor operation mode [0/1] */
} bsec_bme_settings_t;

/* internal defines and backward compatibility */
#define BSEC_STRUCT_NAME Bsec /*!< Internal struct name */

/*@}*/

#ifdef __cplusplus
}
#endif

#endif
