/******************************************************************************
 * \file mtb_xensiv_dps3xx.c
 *
 * \brief
 *     This file contains ModusToolbox&trade; specific functions for the
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

#include "mtb_xensiv_dps3xx.h"

/*******************************************************************************
 * Macros
 *******************************************************************************/
#define I2C_TIMEOUT_MS (1u)
#define I2C_SEND_DATA_SIZE (1u)

/*******************************************************************************
 * Global variables
 *******************************************************************************/
static mtb_hal_i2c_t *_dps368_i2c = NULL;

/*******************************************************************************
 * _xensiv_dps3xx_reg_read
 *******************************************************************************/
static cy_rslt_t _mtb_xensiv_dps3xx_reg_read(void *context, uint16_t timeout, uint8_t i2c_addr, uint8_t reg_addr,
                                             uint8_t *data, uint8_t length) {
  _dps368_i2c = (mtb_hal_i2c_t *) context;
  cy_rslt_t status =
      mtb_hal_i2c_controller_write(_dps368_i2c, i2c_addr, &reg_addr, I2C_SEND_DATA_SIZE, I2C_TIMEOUT_MS, false);
  if (CY_RSLT_SUCCESS == status) {
    status = mtb_hal_i2c_controller_read(_dps368_i2c, i2c_addr, data, length, I2C_TIMEOUT_MS, true);
  }
  return status;
}

/*******************************************************************************
 * _xensiv_dps3xx_reg_write
 *******************************************************************************/
static cy_rslt_t _mtb_xensiv_dps3xx_reg_write(void *context, uint16_t timeout, uint8_t i2c_addr, uint8_t reg_addr,
                                              uint8_t *data, uint8_t length) {
  uint8_t write_data[5];
  if (length > sizeof(write_data)) {
    CY_ASSERT(0);
  }

  _dps368_i2c = (mtb_hal_i2c_t *) context;
  cy_rslt_t status =
      mtb_hal_i2c_controller_write(_dps368_i2c, i2c_addr, &reg_addr, I2C_SEND_DATA_SIZE, I2C_TIMEOUT_MS, false);

  if (CY_RSLT_SUCCESS == status) {
    while (length > 0) {
      status = Cy_SCB_I2C_MasterWriteByte(_dps368_i2c->base, *data, I2C_TIMEOUT_MS, _dps368_i2c->context);

      if (CY_RSLT_SUCCESS != status) {
        break;
      }
      --length;
      ++data;
    }
    /* SCB in I2C mode is very time sensitive. In practice we have to
     * request STOP after each block, otherwise it may break the
     * transmission */
    Cy_SCB_I2C_MasterSendStop(_dps368_i2c->base, I2C_TIMEOUT_MS, _dps368_i2c->context);
  }

  return status;
}

/*******************************************************************************
 * mtb_dps368_delay_us
 *******************************************************************************/
static cy_rslt_t mtb_dps368_delay_us(uint32_t period) {
  mtb_hal_system_delay_us(period);
  return 0;
}

/*******************************************************************************
 * mtb_xensiv_dps3xx_init_i2c
 *******************************************************************************/
cy_rslt_t mtb_xensiv_dps3xx_init_i2c(xensiv_dps3xx_t *obj, mtb_hal_i2c_t *i2c, xensiv_dps3xx_i2c_addr_t i2c_addr) {
  xensiv_dps3xx_i2c_comm_t functions = {.read = _mtb_xensiv_dps3xx_reg_read,
                                        .write = _mtb_xensiv_dps3xx_reg_write,
                                        .delay = mtb_dps368_delay_us,
                                        .context = i2c};
  return xensiv_dps3xx_init_i2c(obj, &functions, i2c_addr);
}

/*******************************************************************************
 * mtb_xensiv_dps3xx_reg_read
 *******************************************************************************/
cy_rslt_t mtb_xensiv_dps3xx_reg_read(xensiv_dps3xx_t *obj, uint8_t reg_addr, uint8_t *data, uint8_t length) {
  return _xensiv_dps3xx_reg_read(obj, reg_addr, data, length);
}

/*******************************************************************************
 * mtb_xensiv_dps3xx_reg_write
 *******************************************************************************/
cy_rslt_t mtb_xensiv_dps3xx_reg_write(xensiv_dps3xx_t *obj, uint8_t reg_addr, uint8_t *data, uint8_t length) {
  return _xensiv_dps3xx_reg_write(obj, reg_addr, data, length);
}

/*******************************************************************************
 * mtb_xensiv_dps3xx_calc_temperature
 *******************************************************************************/
cy_float32_t mtb_xensiv_dps3xx_calc_temperature(xensiv_dps3xx_t *obj, int32_t temp_raw) {
  return _xensiv_dps3xx_calc_temperature(obj, temp_raw);
}

/*******************************************************************************
 * mtb_xensiv_dps3xx_calc_pressure
 *******************************************************************************/
cy_float32_t mtb_xensiv_dps3xx_calc_pressure(xensiv_dps3xx_t *obj, int32_t press_raw) {
  return _xensiv_dps3xx_calc_pressure(obj, press_raw);
}

/* [] END OF FILE */
