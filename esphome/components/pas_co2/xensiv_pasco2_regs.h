/***************************************************************************************************
 * \file xensiv_pasco2_regs.h
 *
 * Description: This file contains the register definitions
 *              for interacting with the XENSIV™ PAS CO2 sensor.
 *
 ***************************************************************************************************
 * \copyright
 * Copyright 2021 Infineon Technologies AG
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **************************************************************************************************/

// clang-format off
// NOLINTBEGIN(*)
#pragma once
#ifndef XENSIV_PASCO2_REGS_H_
#define XENSIV_PASCO2_REGS_H_

#include <stdint.h>

namespace esphome {
namespace pas_co2 {

#define XENSIV_PASCO2_REG_PROD_ID                       (0x00U)                                                 /*!< REG_PROD: Address */
#define XENSIV_PASCO2_REG_SENS_STS                      (0x01U)                                                 /*!< SENS_STS: Address */
#define XENSIV_PASCO2_REG_MEAS_RATE_H                   (0x02U)                                                 /*!< MEAS_RATE_H: Address */
#define XENSIV_PASCO2_REG_MEAS_RATE_L                   (0x03U)                                                 /*!< MEAS_RATE_L: Address */
#define XENSIV_PASCO2_REG_MEAS_CFG                      (0x04U)                                                 /*!< MEAS_CFG: Address */
#define XENSIV_PASCO2_REG_CO2PPM_H                      (0x05U)                                                 /*!< CO2PPM_H: Address */
#define XENSIV_PASCO2_REG_CO2PPM_L                      (0x06U)                                                 /*!< CO2PPM_L: Address */
#define XENSIV_PASCO2_REG_MEAS_STS                      (0x07U)                                                 /*!< MEAS_STS: Address */
#define XENSIV_PASCO2_REG_INT_CFG                       (0x08U)                                                 /*!< INT_CFG: Address */
#define XENSIV_PASCO2_REG_ALARM_TH_H                    (0x09U)                                                 /*!< ALARM_TH_H: Address */
#define XENSIV_PASCO2_REG_ALARM_TH_L                    (0x0aU)                                                 /*!< ALARM_TH_L: Address */
#define XENSIV_PASCO2_REG_PRESS_REF_H                   (0x0bU)                                                 /*!< PRESS_REF_H: Address */
#define XENSIV_PASCO2_REG_PRESS_REF_L                   (0x0cU)                                                 /*!< PRESS_REF_L: Address */
#define XENSIV_PASCO2_REG_CALIB_REF_H                   (0x0dU)                                                 /*!< CALIB_REF_H: Address */
#define XENSIV_PASCO2_REG_CALIB_REF_L                   (0x0eU)                                                 /*!< CALIB_REF_L: Address */
#define XENSIV_PASCO2_REG_SCRATCH_PAD                   (0x0fU)                                                 /*!< SCRATCH_PAD: Address */
#define XENSIV_PASCO2_REG_SENS_RST                      (0x10U)                                                 /*!< SENS_RST: Address */

#define XENSIV_PASCO2_REG_PROD_ID_REV_POS               (0U)                                                    /*!< REG_PROD: ID_REV position */
#define XENSIV_PASCO2_REG_PROD_ID_REV_MSK               (0x1fU << XENSIV_PASCO2_REG_PROD_ID_REV_POS)            /*!< REG_PROD: ID_REV mask */
#define XENSIV_PASCO2_REG_PROD_ID_PROD_POS              (5U)                                                    /*!< REG_PROD: ID_PROD position */
#define XENSIV_PASCO2_REG_PROD_ID_PROD_MSK              (0x07U << XENSIV_PASCO2_REG_PROD_ID_PROD_POS)           /*!< REG_PROD: ID_PROD mask */

#define XENSIV_PASCO2_REG_SENS_STS_ICCER_CLR_POS        (0U)                                                    /*!< SENS_STS: ICCER_CLR position */
#define XENSIV_PASCO2_REG_SENS_STS_ICCER_CLR_MSK        (0x01U << XENSIV_PASCO2_REG_SENS_STS_ICCER_CLR_POS)     /*!< SENS_STS: ICCER_CLR mask */
#define XENSIV_PASCO2_REG_SENS_STS_ORVS_CLR_POS         (1U)                                                    /*!< SENS_STS: ORVS_CLR position */
#define XENSIV_PASCO2_REG_SENS_STS_ORVS_CLR_MSK         (0x01U << XENSIV_PASCO2_REG_SENS_STS_ORVS_CLR_POS)      /*!< SENS_STS: ORVS_CLR mask */
#define XENSIV_PASCO2_REG_SENS_STS_ORTMP_CLR_POS        (2U)                                                    /*!< SENS_STS: ORTMP_CLR position */
#define XENSIV_PASCO2_REG_SENS_STS_ORTMP_CLR_MSK        (0x01U << XENSIV_PASCO2_REG_SENS_STS_ORTMP_CLR_POS)     /*!< SENS_STS: ORTMP_CLR mask */
#define XENSIV_PASCO2_REG_SENS_STS_ICCER_POS            (3U)                                                    /*!< SENS_STS: ICCER position */
#define XENSIV_PASCO2_REG_SENS_STS_ICCER_MSK            (0x01U << XENSIV_PASCO2_REG_SENS_STS_ICCER_POS)         /*!< SENS_STS: ICCER mask */
#define XENSIV_PASCO2_REG_SENS_STS_ORVS_POS             (4U)                                                    /*!< SENS_STS: ORVS position */
#define XENSIV_PASCO2_REG_SENS_STS_ORVS_MSK             (0x01U << XENSIV_PASCO2_REG_SENS_STS_ORVS_POS)          /*!< SENS_STS: ORVS mask */
#define XENSIV_PASCO2_REG_SENS_STS_ORTMP_POS            (5U)                                                    /*!< SENS_STS: ORTMP position */
#define XENSIV_PASCO2_REG_SENS_STS_ORTMP_MSK            (0x01U << XENSIV_PASCO2_REG_SENS_STS_ORTMP_POS)         /*!< SENS_STS: ORTMP mask */
#define XENSIV_PASCO2_REG_SENS_STS_PWM_DIS_ST_POS       (6U)                                                    /*!< SENS_STS: PWM_DIS_ST position */
#define XENSIV_PASCO2_REG_SENS_STS_PWM_DIS_ST_MSK       (0x01U << XENSIV_PASCO2_REG_SENS_STS_PWM_DIS_ST_POS)    /*!< SENS_STS: PWM_DIS_ST mask */
#define XENSIV_PASCO2_REG_SENS_STS_SEN_RDY_POS          (7U)                                                    /*!< SENS_STS: SEN_RDY position */
#define XENSIV_PASCO2_REG_SENS_STS_SEN_RDY_MSK          (0x01U << XENSIV_PASCO2_REG_SENS_STS_SEN_RDY_POS)       /*!< SENS_STS: SEN_RDY mask */

#define XENSIV_PASCO2_REG_MEAS_RATE_H_VAL_POS           (0U)                                                    /*!< MEAS_RATE_H: VAL position */
#define XENSIV_PASCO2_REG_MEAS_RATE_H_VAL_MSK           (0xffU << XENSIV_PASCO2_REG_MEAS_RATE_H_VAL_POS)        /*!< MEAS_RATE_H: VAL mask */

#define XENSIV_PASCO2_REG_MEAS_RATE_L_VAL_POS           (0U)                                                    /*!< MEAS_RATE_L: VAL position */
#define XENSIV_PASCO2_REG_MEAS_RATE_L_VAL_MSK           (0xffU << XENSIV_PASCO2_REG_MEAS_RATE_L_VAL_POS)        /*!< MEAS_RATE_L: VAL mask */

#define XENSIV_PASCO2_REG_MEAS_CFG_OP_MODE_POS          (0U)                                                    /*!< MEAS_CFG: OP_MODE position */
#define XENSIV_PASCO2_REG_MEAS_CFG_OP_MODE_MSK          (0x03U << XENSIV_PASCO2_REG_MEAS_CFG_OP_MODE_POS)       /*!< MEAS_CFG: OP_MODE mask */
#define XENSIV_PASCO2_REG_MEAS_CFG_BOC_CFG_POS          (2U)                                                    /*!< MEAS_CFG: BOC_CFG position */
#define XENSIV_PASCO2_REG_MEAS_CFG_BOC_CFG_MSK          (0x03U << XENSIV_PASCO2_REG_MEAS_CFG_BOC_CFG_POS)       /*!< MEAS_CFG: BOC_CFG mask */
#define XENSIV_PASCO2_REG_MEAS_CFG_PWM_MODE_POS         (4U)                                                    /*!< MEAS_CFG: PWM_MODE position */
#define XENSIV_PASCO2_REG_MEAS_CFG_PWM_MODE_MSK         (0x01U << XENSIV_PASCO2_REG_MEAS_CFG_PWM_MODE_POS)      /*!< MEAS_CFG: PWM_MODE mask */
#define XENSIV_PASCO2_REG_MEAS_CFG_PWM_OUTEN_POS        (5U)                                                    /*!< MEAS_CFG: PWM_OUTEN position */
#define XENSIV_PASCO2_REG_MEAS_CFG_PWM_OUTEN_MSK        (0x01U << XENSIV_PASCO2_REG_MEAS_CFG_PWM_OUTEN_POS)     /*!< MEAS_CFG: PWM_OUTEN mask */

#define XENSIV_PASCO2_REG_CO2PPM_H_VAL_POS              (0U)                                                    /*!< CO2PPM_H: VAL position */
#define XENSIV_PASCO2_REG_CO2PPM_H_VAL_MSK              (0xffU << XENSIV_PASCO2_REG_CO2PPM_H_VAL_POS)           /*!< CO2PPM_H: VAL mask */

#define XENSIV_PASCO2_REG_CO2PPM_L_VAL_POS              (0U)                                                    /*!< CO2PPM_L: VAL position */
#define XENSIV_PASCO2_REG_CO2PPM_L_VAL_MSK              (0xffU << XENSIV_PASCO2_REG_CO2PPM_L_VAL_POS)           /*!< CO2PPM_L: VAL mask */

#define XENSIV_PASCO2_REG_MEAS_STS_ALARM_CLR_POS        (0U)                                                    /*!< MEAS_STS: ALARM_CLR position */
#define XENSIV_PASCO2_REG_MEAS_STS_ALARM_CLR_MSK        (0x01U << XENSIV_PASCO2_REG_MEAS_STS_ALARM_CLR_POS)     /*!< MEAS_STS: ALARM_CLR mask */
#define XENSIV_PASCO2_REG_MEAS_STS_INT_STS_CLR_POS      (1U)                                                    /*!< MEAS_STS: INT_STS_CLR position */
#define XENSIV_PASCO2_REG_MEAS_STS_INT_STS_CLR_MSK      (0x01U << XENSIV_PASCO2_REG_MEAS_STS_INT_STS_CLR_POS)   /*!< MEAS_STS: INT_STS_CLR mask */
#define XENSIV_PASCO2_REG_MEAS_STS_ALARM_POS            (2U)                                                    /*!< MEAS_STS: ALARM position */
#define XENSIV_PASCO2_REG_MEAS_STS_ALARM_MSK            (0x01U << XENSIV_PASCO2_REG_MEAS_STS_ALARM_POS)         /*!< MEAS_STS: ALARM mask */
#define XENSIV_PASCO2_REG_MEAS_STS_INT_STS_POS          (3U)                                                    /*!< MEAS_STS: INT_STS position */
#define XENSIV_PASCO2_REG_MEAS_STS_INT_STS_MSK          (0x01U << XENSIV_PASCO2_REG_MEAS_STS_INT_STS_POS)       /*!< MEAS_STS: INT_STS mask */
#define XENSIV_PASCO2_REG_MEAS_STS_DRDY_POS             (4U)                                                    /*!< MEAS_STS: DRDY position */
#define XENSIV_PASCO2_REG_MEAS_STS_DRDY_MSK             (0x01U << XENSIV_PASCO2_REG_MEAS_STS_DRDY_POS)          /*!< MEAS_STS: DRDY mask */

#define XENSIV_PASCO2_REG_INT_CFG_ALARM_TYP_POS         (0U)                                                    /*!< INT_CFG: ALARM_TYP position */
#define XENSIV_PASCO2_REG_INT_CFG_ALARM_TYP_MSK         (0x01U << XENSIV_PASCO2_REG_INT_CFG_ALARM_TYP_POS)      /*!< INT_CFG: ALARM_TYP mask */
#define XENSIV_PASCO2_REG_INT_CFG_INT_FUNC_POS          (1U)                                                    /*!< INT_CFG: INT_FUNC position */
#define XENSIV_PASCO2_REG_INT_CFG_INT_FUNC_MSK          (0x07U << XENSIV_PASCO2_REG_INT_CFG_INT_FUNC_POS)       /*!< INT_CFG: INT_FUNC mask */
#define XENSIV_PASCO2_REG_INT_CFG_INT_TYP_POS           (4U)                                                    /*!< INT_CFG: INT_TYP position */
#define XENSIV_PASCO2_REG_INT_CFG_INT_TYP_MSK           (0x01U << XENSIV_PASCO2_REG_INT_CFG_INT_TYP_POS)        /*!< INT_CFG: INT_TYP mask */

#define XENSIV_PASCO2_REG_ALARM_TH_H_VAL_POS            (0U)                                                    /*!< ALARM_TH_H: VAL position */
#define XENSIV_PASCO2_REG_ALARM_TH_H_VAL_MSK            (0xffU << XENSIV_PASCO2_REG_ALARM_TH_H_VAL_POS)         /*!< ALARM_TH_H: VAL mask */

#define XENSIV_PASCO2_REG_ALARM_TH_L_VAL_POS            (0U)                                                    /*!< ALARM_TH_L: VAL position */
#define XENSIV_PASCO2_REG_ALARM_TH_L_VAL_MSK            (0xffU << XENSIV_PASCO2_REG_ALARM_TH_L_VAL_POS)         /*!< ALARM_TH_L: VAL mask */

#define XENSIV_PASCO2_REG_PRESS_REF_H_VAL_POS           (0U)                                                    /*!< PRESS_REF_H: VAL position */
#define XENSIV_PASCO2_REG_PRESS_REF_H_VAL_MSK           (0xffU << XENSIV_PASCO2_REG_PRESS_REF_H_VAL_POS)        /*!< PRESS_REF_H: VAL mask */

#define XENSIV_PASCO2_REG_PRESS_REF_L_VAL_POS           (0U)                                                    /*!< PRESS_REF_L: VAL position */
#define XENSIV_PASCO2_REG_PRESS_REF_L_VAL_MSK           (0xffU << XENSIV_PASCO2_REG_PRESS_REF_L_VAL_POS)        /*!< PRESS_REF_L: VAL mask */

#define XENSIV_PASCO2_REG_CALIB_REF_H_VAL_POS           (0U)                                                    /*!< CALIB_REF_H: VAL position */
#define XENSIV_PASCO2_REG_CALIB_REF_H_VAL_MSK           (0xffU << XENSIV_PASCO2_REG_CALIB_REF_H_VAL_POS)        /*!< CALIB_REF_H: VAL mask */

#define XENSIV_PASCO2_REG_CALIB_REF_L_VAL_POS           (0U)                                                    /*!< CALIB_REF_L: VAL position */
#define XENSIV_PASCO2_REG_CALIB_REF_L_VAL_MSK           (0xffU << XENSIV_PASCO2_REG_CALIB_REF_L_VAL_POS)        /*!< CALIB_REF_L: VAL mask */

#define XENSIV_PASCO2_REG_SCRATCH_PAD_VAL_POS           (0U)                                                    /*!< SCRATCH_PAD: VAL position */
#define XENSIV_PASCO2_REG_SCRATCH_PAD_VAL_MSK           (0xffU << XENSIV_PASCO2_REG_SCRATCH_PAD_VAL_POS)        /*!< SCRATCH_PAD: VAL mask */

#define XENSIV_PASCO2_REG_SENS_RST_SRTRG_POS            (0U)                                                    /*!< SENS_RST: SRTRG position */
#define XENSIV_PASCO2_REG_SENS_RST_SRTRG_MSK            (0xffU << XENSIV_PASCO2_REG_SENS_RST_SRTRG_POS)         /*!< SENS_RST: SRTRG mask */

/************************************** Macros *******************************************/

/** Result code indicating a successful operation */
#define XENSIV_PASCO2_OK                    (0)
/** Result code indicating a communication error */
#define XENSIV_PASCO2_ERR_COMM              (1)
/** Result code indicating that an unexpectedly large I2C write was requested which is not supported */
#define XENSIV_PASCO2_ERR_WRITE_TOO_LARGE   (2)
/** Result code indicating that the sensor is not yet ready after reset */
#define XENSIV_PASCO2_ERR_NOT_READY         (3)
/** Result code indicating whether a non-valid command has been received by the serial communication interface */
#define XENSIV_PASCO2_ICCERR                (4)
/** Result code indicating whether a condition where VDD12V has been outside the specified valid range has been detected */
#define XENSIV_PASCO2_ORVS                  (5)
/** Result code indicating whether a condition where the temperature has been outside the specified valid range has been detected */
#define XENSIV_PASCO2_ORTMP                 (6)
/** Result code indicating that a new CO2 value is not yet ready */
#define XENSIV_PASCO2_READ_NRDY             (7)

/** Minimum allowed measurement rate */
#define XENSIV_PASCO2_MEAS_RATE_MIN         (5U)

/** Maximum allowed measurement rate */
#define XENSIV_PASCO2_MEAS_RATE_MAX         (4095U)

/** I2C address of the XENSIV™ PASCO2 sensor */
#define XENSIV_PASCO2_I2C_ADDR              (0x28U)

#define XENSIV_PASCO2_COMM_DELAY_MS             (5U)
#define XENSIV_PASCO2_COMM_TEST_VAL             (0xA5U)

#define XENSIV_PASCO2_SOFT_RESET_DELAY_MS       (2000U)
#define XENSIV_PASCO2_SINGLE_SHOT_DELAY_MS       (5000U)

/********************************* Type definitions **************************************/

/** Enum defining the different device commands */
typedef enum
{
    XENSIV_PASCO2_CMD_SOFT_RESET = 0xA3U,               /**< Soft reset the sensor */
    XENSIV_PASCO2_CMD_RESET_ABOC = 0xBCU,               /**< Resets the ABOC context */
    XENSIV_PASCO2_CMD_SAVE_FCS_CALIB_OFFSET = 0xCFU,    /**< Saves the force calibration offset into the non volatile memory */
    XENSIV_PASCO2_CMD_RESET_FCS = 0xFCU,                /**< Resets the forced calibration correction factor */
} xensiv_pasco2_cmd_t;

/** Enum defining the different device operating modes */
typedef enum
{
    XENSIV_PASCO2_OP_MODE_IDLE = 0U,                    /**< The device does not perform any CO2 concentration measurement */
    XENSIV_PASCO2_OP_MODE_SINGLE = 1U,                  /**< The device triggers a single measurement sequence. At the end of the measurement sequence, the device automatically goes back to idle mode. */
    XENSIV_PASCO2_OP_MODE_CONTINUOUS = 2U               /**< The device periodically triggers a CO2 concentration measurement sequence.
                                                             Once a measurement sequence is completed, the device goes back to an inactive state and wakes
                                                             up automatically for the next measurement sequence. The measurement period can be programmed from 5 seconds to 4095 seconds. */
} xensiv_pasco2_op_mode_t;

/** Enum defining the different device baseline offset compensation (BOC) modes */
typedef enum
{
    XENSIV_PASCO2_BOC_CFG_DISABLE = 0U,                 /**< No offset compensation occurs */
    XENSIV_PASCO2_BOC_CFG_AUTOMATIC = 1U,               /**< The offset is periodically updated at each BOC computation */
    XENSIV_PASCO2_BOC_CFG_FORCED = 2U                   /**< Forced compensation */
} xensiv_pasco2_boc_cfg_t;

/** Enum defining the PWM mode configuration */
typedef enum
{
    XENSIV_PASCO2_PWM_MODE_SINGLE_PULSE = 0U,           /**< PWM single-pulse */
    XENSIV_PASCO2_PWM_MODE_TRAIN_PULSE = 1U             /**< PWM pulse-train mode */
} xensiv_pasco2_pwm_mode_t;

/** Enum defining different interrupt active levels */
typedef enum
{
    XENSIV_PASCO2_INTERRUPT_TYPE_LOW_ACTIVE = 0U,       /**< Pin INT is configured as push-pull and is active LOW */
    XENSIV_PASCO2_INTERRUPT_TYPE_HIGH_ACTIVE = 1U       /**< Pin INT is configured as push-pull and is active HIGH */
} xensiv_pasco2_interrupt_type_t;

/** Enum defining different pin interrupt functions */
typedef enum
{
    XENSIV_PASCO2_INTERRUPT_FUNCTION_NONE = 0U,         /**< Pin INT is inactive */
    XENSIV_PASCO2_INTERRUPT_FUNCTION_ALARM = 1U,        /**< Pin INT is configured as the alarm threshold violation notification pin */
    XENSIV_PASCO2_INTERRUPT_FUNCTION_DRDY = 2U,         /**< Pin INT is configured as the data ready notification pin */
    XENSIV_PASCO2_INTERRUPT_FUNCTION_BUSY = 3U,         /**< Pin INT is configured as the sensor busy notification pin */
    XENSIV_PASCO2_INTERRUPT_FUNCTION_EARLY = 4U         /**< Pin INT is configured as the early measurement start notification pin
                                                             @note This function is available only in continuous mode */
} xensiv_pasco2_interrupt_function_t;

/** Enum defining whether an alarm is issued in the case of a lower or higher threshold violation */
typedef enum
{
    XENSIV_PASCO2_ALARM_TYPE_HIGH_TO_LOW = 0U,          /**< CO2 ppm value falling below the alarm threshold */
    XENSIV_PASCO2_ALARM_TYPE_LOW_TO_HIGH = 1U           /**< CO2 ppm value rising above the alarm threshold */
} xensiv_pasco2_alarm_type_t;

/** Structure of the sensor's product and revision ID register (PROD_ID) */
typedef union
{
  struct
  {
    uint32_t rev:5;                                     /*!< Product and firmware revision */
    uint32_t prod:3;                                    /*!< Product type */
  } b;                                                  /*!< Structure used for bit  access */
  uint8_t u;                                            /*!< Type used for byte access */
} xensiv_pasco2_id_t;

/** Structure of the sensor's status register (SENS_STS) */
typedef union
{
  struct
  {
    uint32_t :3;
    uint32_t iccerr:1;                                  /*!< Communication error notification bit.
                                                             Indicates whether an invalid command has been received by the serial communication interface*/
    uint32_t orvs:1;                                    /*!< Out-of-range VDD12V error bit */
    uint32_t ortmp:1;                                   /*!< Out-of-range temperature error bit */
    uint32_t pwm_dis_st:1;                              /*!< PWM_DIS pin status */
    uint32_t sen_rdy:1;                                 /*!< Sensor ready bit */
  } b;                                                  /*!< Structure used for bit  access */
  uint8_t u;                                            /*!< Type used for byte access */
} xensiv_pasco2_status_t;

/** Structure of the sensor's measurement configuration register (MEAS_CFG) */
typedef union
{
  struct
  {
    uint32_t op_mode:2;                                 /*!< @ref xensiv_pasco2_op_mode_t */
    uint32_t boc_cfg:2;                                 /*!< @ref xensiv_pasco2_boc_cfg_t */
    uint32_t pwm_mode:1;                                /*!< @ref xensiv_pasco2_pwm_mode_t */
    uint32_t pwm_outen:1;                               /*!< PWM output software enable bit */
    uint32_t :2;
  } b;                                                  /*!< Structure used for bit  access */
  uint8_t u;                                            /*!< Type used for byte access */
} xensiv_pasco2_measurement_config_t;

/** Structure of the sensor's interrupt configuration register (INT_CFG) */
typedef union
{
  struct
  {
    uint32_t alarm_typ:1;                               /*!< @ref xensiv_pasco2_alarm_type_t */
    uint32_t int_func:3;                                /*!< @ref xensiv_pasco2_interrupt_function_t */
    uint32_t int_typ:1;                                 /*!< @ref xensiv_pasco2_interrupt_type_t */
    uint32_t :3;
  } b;                                                  /*!< Structure used for bit access */
  uint8_t u;                                            /*!< Type used for byte access */
} xensiv_pasco2_interrupt_config_t;

/** Structure of the sensor's measurement status register (MEAS_STS) */
typedef union
{
  struct
  {
    uint32_t :2;
    uint32_t alarm:1;                                   /*!< Set at the end of every measurement sequence if a threshold violation occurs */
    uint32_t int_sts:1;                                 /*!< Indicates whether the INT pin has been latched to active state (if alarm or data is ready) */
    uint32_t drdy:1;                                    /*!< Indicates whether new data is available */
    uint32_t :3;
  } b;                                                  /*!< Structure used for bit  access */
  uint8_t u;                                            /*!< Type used for byte access */
} xensiv_pasco2_meas_status_t;

#endif

// NOLINTEND(*)
// clang-format on
