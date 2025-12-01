/******************************************************************************
 * \file mtb_xensiv_dps3xx.h
 *
 * \brief
 *     This file contains ModusToolbox&trade; specific public interface for the
 *     XENSIV&trade; DPS3xx pressure sensors.
 *
 ********************************************************************************
 * \copyright
 * Copyright 2021-2025, Cypress Semiconductor Corporation (an Infineon company)
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
 *******************************************************************************/

#pragma once

/**
 * \addtogroup group_board_libs Pressure Sensor
 * \{
 */
#include "mtb_hal.h"
#include "xensiv_dps3xx.h"
#include "cy_result.h"

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

/** Initialize the DPS sensor, and configures it to use the specified I2C peripheral.
 * By default it is configured in command mode.
 *
 * This is equivalent to \ref xensiv_dps3xx_init_i2c() but automatically sets up the function
 * pointers based on the provided I2C object.
 *
 * @param[out]  obj         Pointer to an pressure sensor object. The caller must allocate the
 * memory for this object but the init function will initialize its contents.
 * @param[in]   i2c         Pointer to an initialized I2C object.
 * @param[in]   i2c_addr    I2C address to use when communicating with the sensor.
 * @return CY_RSLT_SUCCESS if properly initialized, else an error indicating what went wrong.
 */
cy_rslt_t mtb_xensiv_dps3xx_init_i2c(xensiv_dps3xx_t *obj, mtb_hal_i2c_t *i2c, xensiv_dps3xx_i2c_addr_t i2c_addr);

/**
 * Read from register
 *
 * @param[in]   obj                 Pointer to a pressure sensor object
 * @param[in]   reg_addr            Indicate register address
 * @param[out]  data                Data to read
 * @param[in]   length              length of data
 * @return CY_RSLT_SUCCESS if the check succeeded, else an error indicating what went wrong.
 */
cy_rslt_t mtb_xensiv_dps3xx_reg_read(xensiv_dps3xx_t *obj, uint8_t reg_addr, uint8_t *data, uint8_t length);

/**
 * Write to register
 *
 * @param[in]   obj                 Pointer to a pressure sensor object
 * @param[in]   reg_addr            Indicate register address
 * @param[out]  data                Data to write
 * @param[in]   length              length of data
 * @return CY_RSLT_SUCCESS if the check succeeded, else an error indicating what went wrong.
 */
cy_rslt_t mtb_xensiv_dps3xx_reg_write(xensiv_dps3xx_t *obj, uint8_t reg_addr, uint8_t *data, uint8_t length);

/**
 * Calculate temperature value from raw temperature data
 *
 * @param[in]   obj                 Pointer to a pressure sensor object
 * @param[in]   temp_raw            Temperature raw data
 *
 * @return cy_float32_t 32-bit single-precision floating-point number
 */
cy_float32_t mtb_xensiv_dps3xx_calc_temperature(xensiv_dps3xx_t *obj, int32_t temp_raw);

/**
 * Calculate pressure value from raw pressure data
 *
 * @param[in]   obj                 Pointer to a pressure sensor object
 * @param[in]   press_raw           Pressure raw data
 *
 * @return cy_float32_t 32-bit single-precision floating-point number
 */
cy_float32_t mtb_xensiv_dps3xx_calc_pressure(xensiv_dps3xx_t *obj, int32_t press_raw);

#if defined(__cplusplus)
}
#endif

/** \} group_board_libs */
