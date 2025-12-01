/***********************************************************************************************/ /**
                                                                                                   * \file
                                                                                                   *xensiv_dps3xx.h
                                                                                                   *
                                                                                                   * Description: This
                                                                                                   *file is the public
                                                                                                   *interface of the
                                                                                                   *XENSIV&trade; DPS3xx
                                                                                                   *pressure sensors.
                                                                                                   ***************************************************************************************************
                                                                                                   * \copyright
                                                                                                   * Copyright 2021-2025
                                                                                                   *Cypress
                                                                                                   *Semiconductor
                                                                                                   *Corporation (an
                                                                                                   *Infineon company) or
                                                                                                   * an affiliate of
                                                                                                   *Cypress
                                                                                                   *Semiconductor
                                                                                                   *Corporation
                                                                                                   *
                                                                                                   * SPDX-License-Identifier:
                                                                                                   *Apache-2.0
                                                                                                   *
                                                                                                   * Licensed under the
                                                                                                   *Apache License,
                                                                                                   *Version 2.0 (the
                                                                                                   *"License"); you may
                                                                                                   *not use this file
                                                                                                   *except in compliance
                                                                                                   *with the License.
                                                                                                   * You may obtain a
                                                                                                   *copy of the License
                                                                                                   *at
                                                                                                   *
                                                                                                   *     http://www.apache.org/licenses/LICENSE-2.0
                                                                                                   *
                                                                                                   * Unless required by
                                                                                                   *applicable law or
                                                                                                   *agreed to in
                                                                                                   *writing, software
                                                                                                   * distributed under
                                                                                                   *the License is
                                                                                                   *distributed on an
                                                                                                   *"AS IS" BASIS,
                                                                                                   * WITHOUT WARRANTIES
                                                                                                   *OR CONDITIONS OF ANY
                                                                                                   *KIND, either express
                                                                                                   *or implied. See the
                                                                                                   *License for the
                                                                                                   *specific language
                                                                                                   *governing
                                                                                                   *permissions and
                                                                                                   * limitations under
                                                                                                   *the License.
                                                                                                   **************************************************************************************************/

#pragma once

/**
 * \addtogroup group_board_libs Pressure Sensor
 * \{
 * Basic set of APIs for interacting with the XENSIV&trade; DPS3xx pressure sensors. This provides
 * basic initialization and access to to the pressure & temperature data. It also provides access
 * to the configuration settings for the sensor for full control. More information about the
 * motion sensor is available at:
 * https://www.infineon.com/products/sensor/pressure-sensors/pressure-sensors-for-iot
 *
 * \note This library uses delays while waiting for the sensor. If the RTOS_AWARE component is set
 * or CY_RTOS_AWARE is defined, the driver will defer to the RTOS for delays. Because of this, it is
 * not safe to call any functions other than \ref xensiv_dps3xx_init_i2c until after the RTOS
 * scheduler
 * has started.
 *
 * \section subsection_board_libs_snippets Code snippets
 * \subsection subsection_board_libs_snippet_1 Snippet 1: Simple initialization with I2C.
 * The following snippet initializes an I2C instance and the DPS3xx.
 * \snippet xensiv_dps3xx_example.c snippet_dps3xx_i2c_init
 *
 * \subsection subsection_board_libs_snippet_2 Snippet 2: Sensor configuration.
 * The following snippet demonstrates how to configure the sensor.
 * \snippet xensiv_dps3xx_example.c snippet_dps3xx_reconfig
 *
 * \subsection subsection_board_libs_snippet_3 Snippet 3: Reading from the sensor.
 * The following snippet demonstrates how to read data from the sensor.
 * \snippet xensiv_dps3xx_example.c snippet_dps3xx_sample
 */

#include "cy_result.h"
#include "cy_utils.h"

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

namespace esphome {
namespace xensiv_dps3xx_base {

/** Result code indicating an unexpectedly large I2C write was requested that is not supported. */
#define XENSIV_DPS3XX_RSLT_ERR_WRITE_TOO_LARGE \
  (CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_BOARD_HARDWARE_DPS3XX, 0x00))
/** Result code indicating a timeout occurred waiting for the pressure sensor data to be ready. */
#define XENSIV_DPS3XX_RSLT_ERR_DATA_NOT_READY \
  (CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_BOARD_HARDWARE_DPS3XX, 0x01))

/** Function pointer for reading sensor registers via I2C. */
typedef cy_rslt_t (*xensiv_dps3xx_i2c_read_t)(void *context, uint16_t timeout, uint8_t i2c_addr, uint8_t reg_adr,
                                              uint8_t *data, uint8_t length);
/** Function pointer for writing sensor registers via I2C */
typedef cy_rslt_t (*xensiv_dps3xx_i2c_write_t)(void *context, uint16_t timeout, uint8_t i2c_addr, uint8_t reg_adr,
                                               uint8_t *data, uint8_t length);
/** Function pointer for delay operations */
typedef cy_rslt_t (*xensiv_dps3xx_delay_t)(uint32_t ms);

/** Possible I2C addresses to communicate with the device */
typedef enum {
  XENSIV_DPS3XX_I2C_ADDR_DEFAULT = 0x77, /**< Default I2C address (0x77) */
  XENSIV_DPS3XX_I2C_ADDR_ALT = 0x76      /**< I2C address (0x76); if SDO pin is pulled to GND */
} xensiv_dps3xx_i2c_addr_t;

// Values here correspond to register field values
/** enum defining the different device operating modes */
typedef enum {
  XENSIV_DPS3XX_MODE_IDLE = 0x00,                   /**< Idle mode value */
  XENSIV_DPS3XX_MODE_COMMAND_PRESSURE = 0x01,       /**< Single shot pressure only. After a
                                                        single measurement the device will
                                                        return to IDLE. Subsequent measurements
                                                        must call \ref xensiv_dps3xx_set_config
                                                        to set the mode again. */
  XENSIV_DPS3XX_MODE_COMMAND_TEMPERATURE = 0x02,    /**< Single shot temperature only. After a
                                                        single measurement the device will
                                                        return to IDLE. Subsequent measurements
                                                        must call \ref xensiv_dps3xx_set_config
                                                        to set the mode again. */
  XENSIV_DPS3XX_MODE_BACKGROUND_PRESSURE = 0x05,    /**< Ongoing background pressure only */
  XENSIV_DPS3XX_MODE_BACKGROUND_TEMPERATURE = 0x06, /**< Ongoing background temperature only */
  XENSIV_DPS3XX_MODE_BACKGROUND_ALL = 0x07          /**< Ongoing background pressure and temp */
} xensiv_dps3xx_mode_t;

/** Enum to enable/disable/report interrupt cause flags. */
typedef enum {
  XENSIV_DPS3XX_INT_NONE = 0x00, /**< No interrupt enabled */
  XENSIV_DPS3XX_INT_HL = 0x80,   /**< Interrupt (on SDO pin) active level */
  XENSIV_DPS3XX_INT_FIFO = 0x40, /**< Interrupt when the FIFO is full */
  XENSIV_DPS3XX_INT_TMP = 0x20,  /**< Interrupt when a temperature measurement is ready */
  XENSIV_DPS3XX_INT_PRS = 0x10   /**< Interrupt when a pressure measurement is ready */
} xensiv_dps3xx_interrupt_t;

// Values here correspond to register field values
/** Oversampling rates for both pressure and temperature measurements */
typedef enum {
  XENSIV_DPS3XX_OVERSAMPLE_1 = 0x00,  /**< 1x oversample rate */
  XENSIV_DPS3XX_OVERSAMPLE_2 = 0x01,  /**< 2x oversample rate */
  XENSIV_DPS3XX_OVERSAMPLE_4 = 0x02,  /**< 4x oversample rate */
  XENSIV_DPS3XX_OVERSAMPLE_8 = 0x03,  /**< 8x oversample rate */
  XENSIV_DPS3XX_OVERSAMPLE_16 = 0x04, /**< 16x oversample rate */
  XENSIV_DPS3XX_OVERSAMPLE_32 = 0x05, /**< 32x oversample rate */
  XENSIV_DPS3XX_OVERSAMPLE_64 = 0x06, /**< 64x oversample rate */
  XENSIV_DPS3XX_OVERSAMPLE_128 = 0x07 /**< 128x oversample rate */
} xensiv_dps3xx_oversample_t;

// Values here correspond to register field values
/** Measurement rates for both pressure and temperature measurements */
typedef enum {
  XENSIV_DPS3XX_RATE_1 = 0x00,  /**< Normal sample rate */
  XENSIV_DPS3XX_RATE_2 = 0x10,  /**< 2x sample rate */
  XENSIV_DPS3XX_RATE_4 = 0x20,  /**< 4x sample rate */
  XENSIV_DPS3XX_RATE_8 = 0x30,  /**< 8x sample rate */
  XENSIV_DPS3XX_RATE_16 = 0x40, /**< 16x sample rate */
  XENSIV_DPS3XX_RATE_32 = 0x50, /**< 32x sample rate */
  XENSIV_DPS3XX_RATE_64 = 0x60, /**< 64x sample rate */
  XENSIV_DPS3XX_RATE_128 = 0x70 /**< 128x sample rate */
} xensiv_dps3xx_rate_t;

/** \cond INTERNAL */
// Values here correspond to register field values
/** Scaling coefficients for both Kp and Kt */
typedef enum {
  _XENSIV_DPS3XX_OSR_SF_1 = 524288,   /**< sensor */
  _XENSIV_DPS3XX_OSR_SF_2 = 1572864,  /**< sensor */
  _XENSIV_DPS3XX_OSR_SF_4 = 3670016,  /**< sensor */
  _XENSIV_DPS3XX_OSR_SF_8 = 7864320,  /**< sensor */
  _XENSIV_DPS3XX_OSR_SF_16 = 253952,  /**< sensor */
  _XENSIV_DPS3XX_OSR_SF_32 = 516096,  /**< sensor */
  _XENSIV_DPS3XX_OSR_SF_64 = 1040384, /**< sensor */
  _XENSIV_DPS3XX_OSR_SF_128 = 2088960 /**< sensor */
} _xensiv_dps3xx_scaling_coeffs_t;

// Values here correspond to register field values
/** Possible sources for temperature data */
typedef enum {
  _XENSIV_DPS3XX_TEMP_SRC_ASIC = 0x00, /**< sensor */
  _XENSIV_DPS3XX_TEMP_SRC_MEMS = 0x80  /**< sensor */
} _xensiv_dps3xx_temp_src_t;

/** Struct to hold calibration coefficients read from device*/
typedef struct {
  /* calibration registers */
  int16_t C0;  /**< 12bit temperature calibration coefficient read from sensor */
  int16_t C1;  /**< 12bit temperature calibration coefficient read from sensor */
  int32_t C00; /**< 12bit pressure calibration coefficient read from sensor */
  int32_t C10; /**< 20bit pressure calibration coefficient read from sensor */
  int32_t C01; /**< 20bit pressure calibration coefficient read from sensor */
  int32_t C11; /**< 20bit pressure calibration coefficient read from sensor */
  int32_t C20; /**< 20bit pressure calibration coefficient read from sensor */
  int32_t C21; /**< 20bit pressure calibration coefficient read from sensor */
  int32_t C30; /**< 20bit pressure calibration coefficient read from sensor */
} _xensiv_dps3xx_cal_coeff_regs_t;
/** \endcond */

/** Structure of configuration parameters for the pressure sensor */
typedef struct {
  xensiv_dps3xx_mode_t dev_mode;                     /**< Operating mode of device */
  xensiv_dps3xx_rate_t pressure_rate;                /**< Pressure sensor measurement mode */
  xensiv_dps3xx_rate_t temperature_rate;             /**< Temperature sensor measurement mode */
  xensiv_dps3xx_oversample_t pressure_oversample;    /**< Pressure sensor oversample rate */
  xensiv_dps3xx_oversample_t temperature_oversample; /**< Temperature sensor oversample rate */
  xensiv_dps3xx_interrupt_t interrupt_triggers;      /**< Signals that trigger an interrupt */
  bool fifo_enable;                                  /**< Should the FIFO buffer be enabled */
  uint16_t data_timeout;                             /**< Millisecond timeout for measurements */
  uint16_t i2c_timeout;                              /**< Millisecond timeout for I2C commands */
} xensiv_dps3xx_config_t;

/** Structure providing function pointers to I2C communication routines. */
typedef struct {
  xensiv_dps3xx_i2c_read_t read;   /**< Function to read sensor registers over I2C */
  xensiv_dps3xx_i2c_write_t write; /**< Function to write sensor registers over I2C */
  xensiv_dps3xx_delay_t delay;     /**< Function to delay while waiting for the sensor */
  void *context;                   /**< Contextual data to provide to the read/write funcs */
} xensiv_dps3xx_i2c_comm_t;

/** Structure of pressure sensor context */
typedef struct {
  xensiv_dps3xx_i2c_comm_t comm;        /**< sensor */
  xensiv_dps3xx_config_t user_config;   /**< User configuration settings */
  xensiv_dps3xx_i2c_addr_t i2c_address; /**< Sensor slave address */

  cy_float32_t temp_scaled;                            /**< sensor */
  uint8_t meas_cfg;                                    /**< sensor */
  _xensiv_dps3xx_scaling_coeffs_t tmp_osr_scale_coeff; /**< Temperature scaling coefficient */
  _xensiv_dps3xx_scaling_coeffs_t prs_osr_scale_coeff; /**< Pressure scaling coefficient */
  _xensiv_dps3xx_cal_coeff_regs_t calib_coeffs;        /**< Calibration coefficients index */
  _xensiv_dps3xx_temp_src_t tmp_ext;                   /**< Should always be set MEMS */
} xensiv_dps3xx_t;

/** Initialize the DPS sensor, and configures it to use the specified I2C peripheral.
 * By default it is configured in command mode.
 *
 * \note A ModusToolbox&trade; HAL based equivalent, mtb_xensiv_dps3xx_init_i2c(), is also available
 *
 * @param[out]  obj         Pointer to an pressure sensor object. The caller must allocate the
 * memory for this object but the init function will initialize its contents.
 * @param[in]   functions   Structure containing pointers to the I2C communication functions.
 * @param[in]   i2c_addr    I2C address to use when communicating with the sensor.
 * @return CY_RSLT_SUCCESS if properly initialized, else an error indicating what went wrong.
 */
cy_rslt_t xensiv_dps3xx_init_i2c(xensiv_dps3xx_t *obj, const xensiv_dps3xx_i2c_comm_t *functions,
                                 xensiv_dps3xx_i2c_addr_t i2c_addr);

/**
 * @brief Gets the current configuration parameters.
 *
 * @param[in]  obj      Pointer to a pressure sensor object.
 * @param[out] config   The current configuration parameters from the device
 * @return CY_RSLT_SUCCESS if the config was obtained, else an error indicating what went wrong.
 */
cy_rslt_t xensiv_dps3xx_get_config(xensiv_dps3xx_t *obj, xensiv_dps3xx_config_t *config);

/**
 * Sets the current configuration parameters for the sensor.
 *
 * @note If switching to a pressure only mode, at least one measurement should be done with
 * temperature data enabled to setup calibration parameters.
 *
 * @param[in]   obj     Pointer to a pressure sensor object.
 * @param[out]  config  The new configuration parameters to apply
 * @return CY_RSLT_SUCCESS if the config was set, else an error indicating what went wrong.
 */
cy_rslt_t xensiv_dps3xx_set_config(xensiv_dps3xx_t *obj, xensiv_dps3xx_config_t *config);

/**
 * Gets the sensor revision number.
 *
 * @param[in]   obj         Pointer to a pressure sensor object.
 * @param[out]  revision_id Pointer to populate with the revision id
 * @return CY_RSLT_SUCCESS if the revision was obtained, else an error indicating what went wrong.
 */
cy_rslt_t xensiv_dps3xx_get_revision_id(xensiv_dps3xx_t *obj, uint8_t *revision_id);

/**
 * Gets the current pressure and temperature values from the sensor.
 *
 * @param[in]   obj         Pointer to a pressure sensor object.
 * @param[out]  pressure    Pointer to populate with the pressure data, may be NULL
 * @param[out]  temperature Pointer to populate with the temperature data, may be NULL
 * @return CY_RSLT_SUCCESS if the data was obtained, else an error indicating what went wrong.
 */
cy_rslt_t xensiv_dps3xx_read(xensiv_dps3xx_t *obj, cy_float32_t *pressure, cy_float32_t *temperature);

/**
 * Gets whether the pressure sensor has new pressure/temperature data ready to be read.
 *
 * @param[in]   obj                 Pointer to a pressure sensor object.
 * @param[out]  pressure_ready      Indication whether the pressure data is ready to be read
 * @param[out]  temperature_ready   Indication whether the temperature data is ready to be read
 * @return CY_RSLT_SUCCESS if the check succeeded, else an error indicating what went wrong.
 */
cy_rslt_t xensiv_dps3xx_check_ready(xensiv_dps3xx_t *obj, bool *pressure_ready, bool *temperature_ready);

/**
 * Frees up any resources allocated by the motion_sensor as part of \ref xensiv_dps3xx_init_i2c().
 *
 * @param[in] obj   Pointer to the pressure sensor object to free.
 */
void xensiv_dps3xx_free(xensiv_dps3xx_t *obj);

/**
 * Read from register
 *
 * @param[in]   obj                 Pointer to a pressure sensor object
 * @param[in]   reg_addr            Indicate register address
 * @param[out]  data                Data to read
 * @param[in]   length              length of data
 * @return CY_RSLT_SUCCESS if the check succeeded, else an error indicating what went wrong.
 */
cy_rslt_t _xensiv_dps3xx_reg_read(xensiv_dps3xx_t *obj, uint8_t reg_addr, uint8_t *data, uint8_t length);

/**
 * Write to register
 *
 * @param[in]   obj                 Pointer to a pressure sensor object
 * @param[in]   reg_addr            Indicate register address
 * @param[out]  data                Data to write
 * @param[in]   length              length of data
 * @return CY_RSLT_SUCCESS if the check succeeded, else an error indicating what went wrong.
 */
cy_rslt_t _xensiv_dps3xx_reg_write(xensiv_dps3xx_t *obj, uint8_t reg_addr, uint8_t *data, uint8_t length);

/**
 * Calculate temperature value from raw temperature data
 *
 * @param[in]   obj                 Pointer to a pressure sensor object
 * @param[in]   temp_raw            Temperature raw data
 *
 * @return cy_float32_t 32-bit single-precision floating-point number
 */
cy_float32_t _xensiv_dps3xx_calc_temperature(xensiv_dps3xx_t *obj, int32_t temp_raw);

/**
 * Calculate pressure value from raw pressure data
 *
 * @param[in]   obj                 Pointer to a pressure sensor object
 * @param[in]   press_raw           Pressure raw data
 *
 * @return cy_float32_t 32-bit single-precision floating-point number
 */
cy_float32_t _xensiv_dps3xx_calc_pressure(xensiv_dps3xx_t *obj, int32_t press_raw);

}  // namespace xensiv_dps3xx_base
}  // namespace esphome

#if defined(__cplusplus)
}
#endif

/** \} group_board_libs */
