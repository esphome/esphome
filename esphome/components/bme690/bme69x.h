/**
* Copyright (c) 2025 Bosch Sensortec GmbH. All rights reserved.
*
* BSD-3-Clause
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its
*    contributors may be used to endorse or promote products derived from
*    this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
* COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
* IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
* @file       bme69x.h
* @date       2025-07-15
* @version    v1.0.3
*
*/

#ifndef BME69X_H_
#define BME69X_H_

#include "bme69x_defs.h"

/* CPP guard */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * \ingroup bme69x
 * \defgroup bme69xApiInit Initialization
 * @brief Initialize the sensor and device structure
 */

/*!
 * \ingroup bme69xApiInit
 * \page bme69x_api_bme69x_init bme69x_init
 * \code
 * int8_t bme69x_init(struct bme69x_dev *dev);
 * \endcode
 * @details This API reads the chip-id of the sensor which is the first step to
 * verify the sensor and also calibrates the sensor
 * As this API is the entry point, call this API before using other APIs.
 *
 * @param[in,out] dev : Structure instance of bme69x_dev
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval < 0 -> Fail
 */
int8_t bme69x_init(struct bme69x_dev *dev);

/**
 * \ingroup bme69x
 * \defgroup bme69xApiRegister Registers
 * @brief Generic API for accessing sensor registers
 */

/*!
 * \ingroup bme69xApiRegister
 * \page bme69x_api_bme69x_set_regs bme69x_set_regs
 * \code
 * int8_t bme69x_set_regs(const uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, struct bme69x_dev *dev)
 * \endcode
 * @details This API writes the given data to the register address of the sensor
 *
 * @param[in] reg_addr : Register addresses to where the data is to be written
 * @param[in] reg_data : Pointer to data buffer which is to be written
 *                       in the reg_addr of sensor.
 * @param[in] len      : No of bytes of data to write
 * @param[in,out] dev  : Structure instance of bme69x_dev
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval < 0 -> Fail
 */
int8_t bme69x_set_regs(const uint8_t *reg_addr, const uint8_t *reg_data, uint32_t len, struct bme69x_dev *dev);

/*!
 * \ingroup bme69xApiRegister
 * \page bme69x_api_bme69x_get_regs bme69x_get_regs
 * \code
 * int8_t bme69x_get_regs(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, struct bme69x_dev *dev)
 * \endcode
 * @details This API reads the data from the given register address of sensor.
 *
 * @param[in] reg_addr  : Register address from where the data to be read
 * @param[out] reg_data : Pointer to data buffer to store the read data.
 * @param[in] len       : No of bytes of data to be read.
 * @param[in,out] dev   : Structure instance of bme69x_dev.
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval < 0 -> Fail
 */
int8_t bme69x_get_regs(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, struct bme69x_dev *dev);

/**
 * \ingroup bme69x
 * \defgroup bme69xApiSystem System
 * @brief API that performs system-level operations
 */

/*!
 * \ingroup bme69xApiSystem
 * \page bme69x_api_bme69x_soft_reset bme69x_soft_reset
 * \code
 * int8_t bme69x_soft_reset(struct bme69x_dev *dev);
 * \endcode
 * @details This API soft-resets the sensor.
 *
 * @param[in,out] dev : Structure instance of bme69x_dev.
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval < 0 -> Fail
 */
int8_t bme69x_soft_reset(struct bme69x_dev *dev);

/**
 * \ingroup bme69x
 * \defgroup bme69xApiOm Operation mode
 * @brief API to configure operation mode
 */

/*!
 * \ingroup bme69xApiOm
 * \page bme69x_api_bme69x_set_op_mode bme69x_set_op_mode
 * \code
 * int8_t bme69x_set_op_mode(const uint8_t op_mode, struct bme69x_dev *dev);
 * \endcode
 * @details This API is used to set the operation mode of the sensor
 * @param[in] op_mode : Desired operation mode.
 * @param[in] dev     : Structure instance of bme69x_dev
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval < 0 -> Fail
 */
int8_t bme69x_set_op_mode(const uint8_t op_mode, struct bme69x_dev *dev);

/*!
 * \ingroup bme69xApiOm
 * \page bme69x_api_bme69x_get_op_mode bme69x_get_op_mode
 * \code
 * int8_t bme69x_get_op_mode(uint8_t *op_mode, struct bme69x_dev *dev);
 * \endcode
 * @details This API is used to get the operation mode of the sensor.
 *
 * @param[out] op_mode : Desired operation mode.
 * @param[in,out] dev : Structure instance of bme69x_dev
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval < 0 -> Fail
 */
int8_t bme69x_get_op_mode(uint8_t *op_mode, struct bme69x_dev *dev);

/*!
 * \ingroup bme69xApiConfig
 * \page bme69x_api_bme69x_get_meas_dur bme69x_get_meas_dur
 * \code
 * uint32_t bme69x_get_meas_dur(const uint8_t op_mode, struct bme69x_conf *conf, struct bme69x_dev *dev);
 * \endcode
 * @details This API is used to get the remaining duration that can be used for heating.
 *
 * @param[in] op_mode : Desired operation mode.
 * @param[in] conf    : Desired sensor configuration.
 * @param[in] dev     : Structure instance of bme69x_dev
 *
 * @return Measurement duration calculated in microseconds
 */
uint32_t bme69x_get_meas_dur(const uint8_t op_mode, struct bme69x_conf *conf, struct bme69x_dev *dev);

/**
 * \ingroup bme69x
 * \defgroup bme69xApiData Data Read out
 * @brief Read our data from the sensor
 */

/*!
 * \ingroup bme69xApiData
 * \page bme69x_api_bme69x_get_data bme69x_get_data
 * \code
 * int8_t bme69x_get_data(uint8_t op_mode, struct bme69x_data *data, uint8_t *n_data, struct bme69x_dev *dev);
 * \endcode
 * @details This API reads the pressure, temperature and humidity and gas data
 * from the sensor, compensates the data and store it in the bme69x_data
 * structure instance passed by the user.
 *
 * @param[in]  op_mode : Expected operation mode.
 * @param[out] data    : Structure instance to hold the data.
 * @param[out] n_data  : Number of data instances available.
 * @param[in,out] dev  : Structure instance of bme69x_dev
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval < 0 -> Fail
 */
int8_t bme69x_get_data(uint8_t op_mode, struct bme69x_data *data, uint8_t *n_data, struct bme69x_dev *dev);

/**
 * \ingroup bme69x
 * \defgroup bme69xApiConfig Configuration
 * @brief Configuration API of sensor
 */

/*!
 * \ingroup bme69xApiConfig
 * \page bme69x_api_bme69x_set_conf bme69x_set_conf
 * \code
 * int8_t bme69x_set_conf(struct bme69x_conf *conf, struct bme69x_dev *dev);
 * \endcode
 * @details This API is used to set the oversampling, filter and odr configuration
 *
 * @param[in] conf    : Desired sensor configuration.
 * @param[in,out] dev : Structure instance of bme69x_dev.
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval < 0 -> Fail
 */
int8_t bme69x_set_conf(struct bme69x_conf *conf, struct bme69x_dev *dev);

/*!
 * \ingroup bme69xApiConfig
 * \page bme69x_api_bme69x_get_conf bme69x_get_conf
 * \code
 * int8_t bme69x_get_conf(struct bme69x_conf *conf, struct bme69x_dev *dev);
 * \endcode
 * @details This API is used to get the oversampling, filter and odr
 * configuration
 *
 * @param[out] conf   : Present sensor configuration.
 * @param[in,out] dev : Structure instance of bme69x_dev.
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval < 0 -> Fail
 */
int8_t bme69x_get_conf(struct bme69x_conf *conf, struct bme69x_dev *dev);

/*!
 * \ingroup bme69xApiConfig
 * \page bme69x_api_bme69x_set_heatr_conf bme69x_set_heatr_conf
 * \code
 * int8_t bme69x_set_heatr_conf(uint8_t op_mode, const struct bme69x_heatr_conf *conf, struct bme69x_dev *dev);
 * \endcode
 * @details This API is used to set the gas configuration of the sensor.
 *
 * @param[in] op_mode : Expected operation mode of the sensor.
 * @param[in] conf    : Desired heating configuration.
 * @param[in,out] dev : Structure instance of bme69x_dev.
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval < 0 -> Fail
 */
int8_t bme69x_set_heatr_conf(uint8_t op_mode, const struct bme69x_heatr_conf *conf, struct bme69x_dev *dev);

/*!
 * \ingroup bme69xApiConfig
 * \page bme69x_api_bme69x_get_heatr_conf bme69x_get_heatr_conf
 * \code
 * int8_t bme69x_get_heatr_conf(const struct bme69x_heatr_conf *conf, struct bme69x_dev *dev);
 * \endcode
 * @details This API is used to get the gas configuration of the sensor.
 *
 * @param[out] conf   : Current configurations of the gas sensor.
 * @param[in,out] dev : Structure instance of bme69x_dev.
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval < 0 -> Fail
 */
int8_t bme69x_get_heatr_conf(const struct bme69x_heatr_conf *conf, struct bme69x_dev *dev);

/*!
 * \ingroup bme69xApiSystem
 * \page bme69x_api_bme69x_selftest_check bme69x_selftest_check
 * \code
 * int8_t bme69x_selftest_check(const struct bme69x_dev *dev);
 * \endcode
 * @details This API performs Self-test of low gas variant of BME69X
 *
 * @param[in, out]   dev  : Structure instance of bme69x_dev
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval < 0 -> Fail
 */
int8_t bme69x_selftest_check(const struct bme69x_dev *dev);

#ifdef __cplusplus
}
#endif /* End of CPP guard */
#endif /* BME69X_H_ */
