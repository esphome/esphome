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
* @file       bme69x.c
* @date       2025-07-15
* @version    v1.0.3
*
*/

#include "bme69x.h"
#include <stdio.h>

/* This internal API is used to read the calibration coefficients */
static int8_t get_calib_data(struct bme69x_dev *dev);

/* This internal API is used to read variant ID information register status */
static int8_t read_variant_id(struct bme69x_dev *dev);

/* This internal API is used to calculate the gas wait */
static uint8_t calc_gas_wait(uint16_t dur);

#ifndef BME69X_USE_FPU

/* This internal API is used to calculate the temperature in integer */
static int16_t calc_temperature(uint32_t temp_adc, struct bme69x_dev *dev, uint32_t *t_lin);

/* This internal API is used to calculate the pressure in integer */
static uint32_t calc_pressure(uint32_t pres_adc, uint32_t t_lin, const struct bme69x_dev *dev);

/* This internal API is used to calculate the humidity in integer */
static uint32_t calc_humidity(uint16_t hum_adc, int16_t comp_temperature, const struct bme69x_dev *dev);

/* This internal API is used to calculate the gas resistance for BME69x variant */
static uint32_t calc_gas_resistance(uint16_t gas_res_adc, uint8_t gas_range);

/* This internal API is used to calculate the heater resistance using integer */
static uint8_t calc_res_heat(uint16_t temp, const struct bme69x_dev *dev);

#else

/* This internal API is used to calculate the temperature value in float */
static float calc_temperature(uint32_t temp_adc, const struct bme69x_dev *dev);

/* This internal API is used to calculate the pressure value in float */
static float calc_pressure(uint32_t pres_adc, float comp_temperature, const struct bme69x_dev *dev);

/* This internal API is used to calculate the humidity value in float */
static float calc_humidity(uint16_t hum_adc, float comp_temperature, const struct bme69x_dev *dev);

/* This internal API is used to calculate the gas for BME69x variant in float */
static float calc_gas_resistance(uint16_t gas_res_adc, uint8_t gas_range);

/* This internal API is used to calculate the heater resistance value using float */
static uint8_t calc_res_heat(uint16_t temp, const struct bme69x_dev *dev);

#endif

/* This internal API is used to read a single data of the sensor */
static int8_t read_field_data(uint8_t index, struct bme69x_data *data, struct bme69x_dev *dev);

/* This internal API is used to read all data fields of the sensor */
static int8_t read_all_field_data(struct bme69x_data * const data[], struct bme69x_dev *dev);

/* This internal API is used to switch between SPI memory pages */
static int8_t set_mem_page(uint8_t reg_addr, struct bme69x_dev *dev);

/* This internal API is used to get the current SPI memory page */
static int8_t get_mem_page(struct bme69x_dev *dev);

/* This internal API is used to check the bme69x_dev for null pointers */
static int8_t null_ptr_check(const struct bme69x_dev *dev);

/* This internal API is used to set heater configurations */
static int8_t set_conf(const struct bme69x_heatr_conf *conf, uint8_t op_mode, uint8_t *nb_conv, struct bme69x_dev *dev);

/* This internal API is used to limit the max value of a parameter */
static int8_t boundary_check(uint8_t *value, uint8_t max, struct bme69x_dev *dev);

/* This internal API is used to calculate the register value for
 * shared heater duration */
static uint8_t calc_heatr_dur_shared(uint16_t dur);

/* This internal API is used to swap two fields */
static void swap_fields(uint8_t index1, uint8_t index2, struct bme69x_data *field[]);

/* This internal API is used sort the sensor data */
static void sort_sensor_data(uint8_t low_index, uint8_t high_index, struct bme69x_data *field[]);

/*
 * @brief       Function to analyze the sensor data
 *
 * @param[in]   data    Array of measurement data
 * @param[in]   n_meas  Number of measurements
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval < 0 -> Fail
 */
static int8_t analyze_sensor_data(const struct bme69x_data *data, uint8_t n_meas);

/******************************************************************************************/
/*                                 Global API definitions                                 */
/******************************************************************************************/

/* @brief This API reads the chip-id of the sensor which is the first step to
* verify the sensor and also calibrates the sensor
* As this API is the entry point, call this API before using other APIs.
*/
int8_t bme69x_init(struct bme69x_dev *dev)
{
    int8_t rslt;

    (void) bme69x_soft_reset(dev);

    rslt = bme69x_get_regs(BME69X_REG_CHIP_ID, &dev->chip_id, 1, dev);

    if (rslt == BME69X_OK)
    {
        if (dev->chip_id == BME69X_CHIP_ID)
        {
            /* Read Variant ID */
            rslt = read_variant_id(dev);

            if (rslt == BME69X_OK)
            {
                /* Get the Calibration data */
                rslt = get_calib_data(dev);
            }
        }
        else
        {
            rslt = BME69X_E_DEV_NOT_FOUND;
        }
    }

    return rslt;
}

/*
 * @brief This API writes the given data to the register address of the sensor
 */
int8_t bme69x_set_regs(const uint8_t *reg_addr, const uint8_t *reg_data, uint32_t len, struct bme69x_dev *dev)
{
    int8_t rslt;

    /* Length of the temporary buffer is 2*(length of register)*/
    uint8_t tmp_buff[BME69X_LEN_INTERLEAVE_BUFF] = { 0 };
    uint16_t index;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    if ((rslt == BME69X_OK) && reg_addr && reg_data)
    {
        if ((len > 0) && (len <= (BME69X_LEN_INTERLEAVE_BUFF / 2)))
        {
            /* Interleave the 2 arrays */
            for (index = 0; index < len; index++)
            {
                if (dev->intf == BME69X_SPI_INTF)
                {
                    /* Set the memory page */
                    rslt = set_mem_page(reg_addr[index], dev);
                    tmp_buff[(2 * index)] = reg_addr[index] & BME69X_SPI_WR_MSK;
                }
                else
                {
                    tmp_buff[(2 * index)] = reg_addr[index];
                }

                tmp_buff[(2 * index) + 1] = reg_data[index];
            }

            /* Write the interleaved array */
            if (rslt == BME69X_OK)
            {
                dev->intf_rslt = dev->write(tmp_buff[0], &tmp_buff[1], (2 * len) - 1, dev->intf_ptr);
                if (dev->intf_rslt != 0)
                {
                    rslt = BME69X_E_COM_FAIL;
                }
            }
        }
        else
        {
            rslt = BME69X_E_INVALID_LENGTH;
        }
    }
    else
    {
        rslt = BME69X_E_NULL_PTR;
    }

    return rslt;
}

/*
 * @brief This API reads the data from the given register address of sensor.
 */
int8_t bme69x_get_regs(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, struct bme69x_dev *dev)
{
    int8_t rslt;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    if ((rslt == BME69X_OK) && reg_data)
    {
        if (dev->intf == BME69X_SPI_INTF)
        {
            /* Set the memory page */
            rslt = set_mem_page(reg_addr, dev);
            if (rslt == BME69X_OK)
            {
                reg_addr = reg_addr | BME69X_SPI_RD_MSK;
            }
        }

        dev->intf_rslt = dev->read(reg_addr, reg_data, len, dev->intf_ptr);
        if (dev->intf_rslt != 0)
        {
            rslt = BME69X_E_COM_FAIL;
        }
    }
    else
    {
        rslt = BME69X_E_NULL_PTR;
    }

    return rslt;
}

/*
 * @brief This API soft-resets the sensor.
 */
int8_t bme69x_soft_reset(struct bme69x_dev *dev)
{
    int8_t rslt;
    uint8_t reg_addr = BME69X_REG_SOFT_RESET;

    /* 0xb6 is the soft reset command */
    uint8_t soft_rst_cmd = BME69X_SOFT_RESET_CMD;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    if (rslt == BME69X_OK)
    {
        if (dev->intf == BME69X_SPI_INTF)
        {
            rslt = get_mem_page(dev);
        }

        /* Reset the device */
        if (rslt == BME69X_OK)
        {
            rslt = bme69x_set_regs(&reg_addr, &soft_rst_cmd, 1, dev);

            if (rslt == BME69X_OK)
            {
                /* Wait for 5ms */
                dev->delay_us(BME69X_PERIOD_RESET, dev->intf_ptr);

                /* After reset get the memory page */
                if (dev->intf == BME69X_SPI_INTF)
                {
                    rslt = get_mem_page(dev);
                }
            }
        }
    }

    return rslt;
}

/*
 * @brief This API is used to set the oversampling, filter and odr configuration
 */
int8_t bme69x_set_conf(struct bme69x_conf *conf, struct bme69x_dev *dev)
{
    int8_t rslt;
    uint8_t odr20 = 0, odr3 = 1;
    uint8_t current_op_mode;

    /* Register data starting from BME69X_REG_CTRL_GAS_1(0x71) up to BME69X_REG_CONFIG(0x75) */
    uint8_t reg_array[BME69X_LEN_CONFIG] = { 0x71, 0x72, 0x73, 0x74, 0x75 };
    uint8_t data_array[BME69X_LEN_CONFIG] = { 0 };

    rslt = bme69x_get_op_mode(&current_op_mode, dev);
    if (rslt == BME69X_OK)
    {
        /* Configure only in the sleep mode */
        rslt = bme69x_set_op_mode(BME69X_SLEEP_MODE, dev);
    }

    if (conf == NULL)
    {
        rslt = BME69X_E_NULL_PTR;
    }
    else if (rslt == BME69X_OK)
    {
        /* Read the whole configuration and write it back once later */
        rslt = bme69x_get_regs(reg_array[0], data_array, BME69X_LEN_CONFIG, dev);
        dev->info_msg = BME69X_OK;
        if (rslt == BME69X_OK)
        {
            rslt = boundary_check(&conf->filter, BME69X_FILTER_SIZE_127, dev);
        }

        if (rslt == BME69X_OK)
        {
            rslt = boundary_check(&conf->os_temp, BME69X_OS_16X, dev);
        }

        if (rslt == BME69X_OK)
        {
            rslt = boundary_check(&conf->os_pres, BME69X_OS_16X, dev);
        }

        if (rslt == BME69X_OK)
        {
            rslt = boundary_check(&conf->os_hum, BME69X_OS_16X, dev);
        }

        if (rslt == BME69X_OK)
        {
            rslt = boundary_check(&conf->odr, BME69X_ODR_NONE, dev);
        }

        if (rslt == BME69X_OK)
        {
            data_array[4] = BME69X_SET_BITS(data_array[4], BME69X_FILTER, conf->filter);
            data_array[3] = BME69X_SET_BITS(data_array[3], BME69X_OST, conf->os_temp);
            data_array[3] = BME69X_SET_BITS(data_array[3], BME69X_OSP, conf->os_pres);
            data_array[1] = BME69X_SET_BITS_POS_0(data_array[1], BME69X_OSH, conf->os_hum);
            if (conf->odr != BME69X_ODR_NONE)
            {
                odr20 = conf->odr;
                odr3 = 0;
            }

            data_array[4] = BME69X_SET_BITS(data_array[4], BME69X_ODR20, odr20);
            data_array[0] = BME69X_SET_BITS(data_array[0], BME69X_ODR3, odr3);
        }
    }

    if (rslt == BME69X_OK)
    {
        rslt = bme69x_set_regs(reg_array, data_array, BME69X_LEN_CONFIG, dev);
    }

    if ((current_op_mode != BME69X_SLEEP_MODE) && (rslt == BME69X_OK))
    {
        rslt = bme69x_set_op_mode(current_op_mode, dev);
    }

    return rslt;
}

/*
 * @brief This API is used to get the oversampling, filter and odr
 */
int8_t bme69x_get_conf(struct bme69x_conf *conf, struct bme69x_dev *dev)
{
    int8_t rslt;

    /* starting address of the register array for burst read*/
    uint8_t reg_addr = BME69X_REG_CTRL_GAS_1;
    uint8_t data_array[BME69X_LEN_CONFIG];

    rslt = bme69x_get_regs(reg_addr, data_array, 5, dev);
    if (!conf)
    {
        rslt = BME69X_E_NULL_PTR;
    }
    else if (rslt == BME69X_OK)
    {
        conf->os_hum = BME69X_GET_BITS_POS_0(data_array[1], BME69X_OSH);
        conf->filter = BME69X_GET_BITS(data_array[4], BME69X_FILTER);
        conf->os_temp = BME69X_GET_BITS(data_array[3], BME69X_OST);
        conf->os_pres = BME69X_GET_BITS(data_array[3], BME69X_OSP);
        if (BME69X_GET_BITS(data_array[0], BME69X_ODR3))
        {
            conf->odr = BME69X_ODR_NONE;
        }
        else
        {
            conf->odr = BME69X_GET_BITS(data_array[4], BME69X_ODR20);
        }
    }

    return rslt;
}

/*
 * @brief This API is used to set the operation mode of the sensor
 */
int8_t bme69x_set_op_mode(const uint8_t op_mode, struct bme69x_dev *dev)
{
    int8_t rslt;
    uint8_t tmp_pow_mode;
    uint8_t pow_mode = 0;
    uint8_t reg_addr = BME69X_REG_CTRL_MEAS;

    /* Call until in sleep */
    do
    {
        rslt = bme69x_get_regs(BME69X_REG_CTRL_MEAS, &tmp_pow_mode, 1, dev);
        if (rslt == BME69X_OK)
        {
            /* Put to sleep before changing mode */
            pow_mode = (tmp_pow_mode & BME69X_MODE_MSK);
            if (pow_mode != BME69X_SLEEP_MODE)
            {
                tmp_pow_mode &= ~BME69X_MODE_MSK; /* Set to sleep */
                rslt = bme69x_set_regs(&reg_addr, &tmp_pow_mode, 1, dev);
                dev->delay_us(BME69X_PERIOD_POLL, dev->intf_ptr);
            }
        }
    } while ((pow_mode != BME69X_SLEEP_MODE) && (rslt == BME69X_OK));

    /* Already in sleep */
    if ((op_mode != BME69X_SLEEP_MODE) && (rslt == BME69X_OK))
    {
        tmp_pow_mode = (tmp_pow_mode & ~BME69X_MODE_MSK) | (op_mode & BME69X_MODE_MSK);
        rslt = bme69x_set_regs(&reg_addr, &tmp_pow_mode, 1, dev);
    }

    return rslt;
}

/*
 * @brief This API is used to get the operation mode of the sensor.
 */
int8_t bme69x_get_op_mode(uint8_t *op_mode, struct bme69x_dev *dev)
{
    int8_t rslt;
    uint8_t mode;

    if (op_mode)
    {
        rslt = bme69x_get_regs(BME69X_REG_CTRL_MEAS, &mode, 1, dev);

        /* Masking the other register bit info*/
        *op_mode = mode & BME69X_MODE_MSK;
    }
    else
    {
        rslt = BME69X_E_NULL_PTR;
    }

    return rslt;
}

/*
 * @brief This API is used to get the remaining duration that can be used for heating.
 */
uint32_t bme69x_get_meas_dur(const uint8_t op_mode, struct bme69x_conf *conf, struct bme69x_dev *dev)
{
    int8_t rslt;
    uint32_t meas_dur = 0; /* Calculate in us */
    uint32_t meas_cycles;
    uint8_t os_to_meas_cycles[6] = { 0, 1, 2, 4, 8, 16 };

    if (conf != NULL)
    {
        /* Boundary check for temperature oversampling */
        rslt = boundary_check(&conf->os_temp, BME69X_OS_16X, dev);

        if (rslt == BME69X_OK)
        {
            /* Boundary check for pressure oversampling */
            rslt = boundary_check(&conf->os_pres, BME69X_OS_16X, dev);
        }

        if (rslt == BME69X_OK)
        {
            /* Boundary check for humidity oversampling */
            rslt = boundary_check(&conf->os_hum, BME69X_OS_16X, dev);
        }

        if (rslt == BME69X_OK)
        {
            meas_cycles = os_to_meas_cycles[conf->os_temp];
            meas_cycles += os_to_meas_cycles[conf->os_pres];
            meas_cycles += os_to_meas_cycles[conf->os_hum];

            /* TPH measurement duration */
            meas_dur = meas_cycles * UINT32_C(1963);
            meas_dur += UINT32_C(477 * 4); /* TPH switching duration */
            meas_dur += UINT32_C(477 * 5); /* Gas measurement duration */

            if (op_mode != BME69X_PARALLEL_MODE)
            {
                meas_dur += UINT32_C(1000); /* Wake up duration of 1ms */
            }
        }
    }

    return meas_dur;
}

/*
 * @brief This API reads the pressure, temperature and humidity and gas data
 * from the sensor, compensates the data and store it in the bme69x_data
 * structure instance passed by the user.
 */
int8_t bme69x_get_data(uint8_t op_mode, struct bme69x_data *data, uint8_t *n_data, struct bme69x_dev *dev)
{
    int8_t rslt;
    uint8_t i = 0, j = 0, new_fields = 0;
    struct bme69x_data *field_ptr[3] = { 0 };
    struct bme69x_data field_data[3] = { { 0 } };

    field_ptr[0] = &field_data[0];
    field_ptr[1] = &field_data[1];
    field_ptr[2] = &field_data[2];

    rslt = null_ptr_check(dev);
    if ((rslt == BME69X_OK) && (data != NULL))
    {
        /* Reading the sensor data in forced mode only */
        if (op_mode == BME69X_FORCED_MODE)
        {
            rslt = read_field_data(0, data, dev);
            if (rslt == BME69X_OK)
            {
                if (data->status & BME69X_NEW_DATA_MSK)
                {
                    new_fields = 1;
                }
                else
                {
                    new_fields = 0;
                    rslt = BME69X_W_NO_NEW_DATA;
                }
            }
        }
        else if ((op_mode == BME69X_PARALLEL_MODE) || (op_mode == BME69X_SEQUENTIAL_MODE))
        {
            /* Read the 3 fields and count the number of new data fields */
            rslt = read_all_field_data(field_ptr, dev);

            new_fields = 0;
            for (i = 0; (i < 3) && (rslt == BME69X_OK); i++)
            {
                if (field_ptr[i]->status & BME69X_NEW_DATA_MSK)
                {
                    new_fields++;
                }
            }

            /* Sort the sensor data in parallel & sequential modes*/
            for (i = 0; (i < 2) && (rslt == BME69X_OK); i++)
            {
                for (j = i + 1; j < 3; j++)
                {
                    sort_sensor_data(i, j, field_ptr);
                }
            }

            /* Copy the sorted data */
            for (i = 0; ((i < 3) && (rslt == BME69X_OK)); i++)
            {
                data[i] = *field_ptr[i];
            }

            if (new_fields == 0)
            {
                rslt = BME69X_W_NO_NEW_DATA;
            }
        }
        else
        {
            rslt = BME69X_W_DEFINE_OP_MODE;
        }

        if (n_data == NULL)
        {
            rslt = BME69X_E_NULL_PTR;
        }
        else
        {
            *n_data = new_fields;
        }
    }
    else
    {
        rslt = BME69X_E_NULL_PTR;
    }

    return rslt;
}

/*
 * @brief This API is used to set the gas configuration of the sensor.
 */
int8_t bme69x_set_heatr_conf(uint8_t op_mode, const struct bme69x_heatr_conf *conf, struct bme69x_dev *dev)
{
    int8_t rslt;
    uint8_t nb_conv = 0;
    uint8_t hctrl, run_gas = 0;
    uint8_t ctrl_gas_data[2];
    uint8_t ctrl_gas_addr[2] = { BME69X_REG_CTRL_GAS_0, BME69X_REG_CTRL_GAS_1 };

    if (conf != NULL)
    {
        rslt = bme69x_set_op_mode(BME69X_SLEEP_MODE, dev);
        if (rslt == BME69X_OK)
        {
            rslt = set_conf(conf, op_mode, &nb_conv, dev);
        }

        if (rslt == BME69X_OK)
        {
            rslt = bme69x_get_regs(BME69X_REG_CTRL_GAS_0, ctrl_gas_data, 2, dev);
            if (rslt == BME69X_OK)
            {
                if (conf->enable == BME69X_ENABLE)
                {
                    hctrl = BME69X_ENABLE_HEATER;
                    run_gas = BME69X_ENABLE_GAS_MEAS;

                }
                else
                {
                    hctrl = BME69X_DISABLE_HEATER;
                    run_gas = BME69X_DISABLE_GAS_MEAS;
                }

                ctrl_gas_data[0] = BME69X_SET_BITS(ctrl_gas_data[0], BME69X_HCTRL, hctrl);
                ctrl_gas_data[1] = BME69X_SET_BITS_POS_0(ctrl_gas_data[1], BME69X_NBCONV, nb_conv);
                ctrl_gas_data[1] = BME69X_SET_BITS(ctrl_gas_data[1], BME69X_RUN_GAS, run_gas);

                rslt = bme69x_set_regs(ctrl_gas_addr, ctrl_gas_data, 2, dev);
            }
        }
    }
    else
    {
        rslt = BME69X_E_NULL_PTR;
    }

    return rslt;
}

/*!
 * @brief This API is used to get the gas configuration of the sensor.
 */
int8_t bme69x_get_heatr_conf(const struct bme69x_heatr_conf *conf, struct bme69x_dev *dev)
{
    int8_t rslt = BME69X_OK;
    uint8_t data_array[10] = { 0 };
    uint8_t i;

    if ((conf != NULL) && (conf->heatr_dur_prof != NULL) && (conf->heatr_temp_prof != NULL))
    {
        /* FIXME: Add conversion to deg C and ms and add the other parameters */
        rslt = bme69x_get_regs(BME69X_REG_RES_HEAT0, data_array, 10, dev);

        if (rslt == BME69X_OK)
        {
            for (i = 0; i < conf->profile_len; i++)
            {
                conf->heatr_temp_prof[i] = data_array[i];
            }

            rslt = bme69x_get_regs(BME69X_REG_GAS_WAIT0, data_array, 10, dev);

            if (rslt == BME69X_OK)
            {
                for (i = 0; i < conf->profile_len; i++)
                {
                    conf->heatr_dur_prof[i] = data_array[i];
                }
            }
        }
    }
    else
    {
        rslt = BME69X_E_NULL_PTR;
    }

    return rslt;
}

/*
 * @brief This API performs Self-test of low and high gas variants of BME69X
 */
int8_t bme69x_selftest_check(const struct bme69x_dev *dev)
{
    int8_t rslt;
    uint8_t n_fields;
    uint8_t i = 0;
    struct bme69x_data data[BME69X_N_MEAS] = { { 0 } };
    struct bme69x_dev t_dev;
    struct bme69x_conf conf;
    struct bme69x_heatr_conf heatr_conf;

    rslt = null_ptr_check(dev);

    if (rslt == BME69X_OK)
    {
        /* Copy required parameters from reference bme69x_dev struct */
        t_dev.amb_temp = 25;
        t_dev.read = dev->read;
        t_dev.write = dev->write;
        t_dev.intf = dev->intf;
        t_dev.delay_us = dev->delay_us;
        t_dev.intf_ptr = dev->intf_ptr;

        rslt = bme69x_init(&t_dev);
    }

    if (rslt == BME69X_OK)
    {
        /* Set the temperature, pressure and humidity & filter settings */
        conf.os_hum = BME69X_OS_1X;
        conf.os_pres = BME69X_OS_16X;
        conf.os_temp = BME69X_OS_2X;

        /* Set the remaining gas sensor settings and link the heating profile */
        heatr_conf.enable = BME69X_ENABLE;
        heatr_conf.heatr_dur = BME69X_HEATR_DUR1;
        heatr_conf.heatr_temp = BME69X_HIGH_TEMP;
        rslt = bme69x_set_heatr_conf(BME69X_FORCED_MODE, &heatr_conf, &t_dev);
        if (rslt == BME69X_OK)
        {
            rslt = bme69x_set_conf(&conf, &t_dev);
            if (rslt == BME69X_OK)
            {
                rslt = bme69x_set_op_mode(BME69X_FORCED_MODE, &t_dev); /* Trigger a measurement */
                if (rslt == BME69X_OK)
                {
                    /* Wait for the measurement to complete */
                    t_dev.delay_us(BME69X_HEATR_DUR1_DELAY, t_dev.intf_ptr);
                    rslt = bme69x_get_data(BME69X_FORCED_MODE, &data[0], &n_fields, &t_dev);
                    if (rslt == BME69X_OK)
                    {
                        if ((data[0].idac != 0x00) && (data[0].idac != 0xFF) &&
                            (data[0].status & BME69X_GASM_VALID_MSK))
                        {
                            rslt = BME69X_OK;
                        }
                        else
                        {
                            rslt = BME69X_E_SELF_TEST;
                        }
                    }
                }
            }
        }

        heatr_conf.heatr_dur = BME69X_HEATR_DUR2;
        while ((rslt == BME69X_OK) && (i < BME69X_N_MEAS))
        {
            if (i % 2 == 0)
            {
                heatr_conf.heatr_temp = BME69X_HIGH_TEMP; /* Higher temperature */
            }
            else
            {
                heatr_conf.heatr_temp = BME69X_LOW_TEMP; /* Lower temperature */
            }

            rslt = bme69x_set_heatr_conf(BME69X_FORCED_MODE, &heatr_conf, &t_dev);
            if (rslt == BME69X_OK)
            {
                rslt = bme69x_set_conf(&conf, &t_dev);
                if (rslt == BME69X_OK)
                {
                    rslt = bme69x_set_op_mode(BME69X_FORCED_MODE, &t_dev); /* Trigger a measurement */
                    if (rslt == BME69X_OK)
                    {
                        /* Wait for the measurement to complete */
                        t_dev.delay_us(BME69X_HEATR_DUR2_DELAY, t_dev.intf_ptr);
                        rslt = bme69x_get_data(BME69X_FORCED_MODE, &data[i], &n_fields, &t_dev);
                    }
                }
            }

            i++;
        }

        if (rslt == BME69X_OK)
        {
            rslt = analyze_sensor_data(data, BME69X_N_MEAS);
        }
    }

    return rslt;
}

/*****************************INTERNAL APIs***********************************************/
#ifndef BME69X_USE_FPU

/* @brief This internal API is used to calculate the temperature value. */
static int16_t calc_temperature(uint32_t temp_adc, struct bme69x_dev *dev, uint32_t *t_lin)
{
    int64_t partial_data1;
    int64_t partial_data2;
    int64_t partial_data3;
    int64_t partial_data4;
    int64_t partial_data5;
    int64_t partial_data6;
    int64_t tem_comp;

    partial_data1 = (int64_t)(temp_adc - (256U * dev->calib.par_t1));
    partial_data2 = (int64_t)(partial_data1 * (int64_t)dev->calib.par_t2);
    partial_data3 = (int64_t)(partial_data1 * partial_data1);
    partial_data4 = (int64_t)(partial_data3 * (int64_t)dev->calib.par_t3);
    partial_data5 = (int64_t)((int64_t)(partial_data2 * 262144UL) + partial_data4);
    partial_data6 = (int64_t)(partial_data5 / 4294967296ULL);
    *t_lin = (uint32_t)partial_data6;
    tem_comp = (int64_t)((partial_data6 * 25U) / 16384UL);

    return (int16_t)(tem_comp);
}

/* @brief This internal API is used to calculate the pressure value. */
static uint32_t calc_pressure(uint32_t pres_adc, uint32_t t_lin, const struct bme69x_dev *dev)
{
    int64_t partial_data1;
    int64_t partial_data2;
    int64_t partial_data3;
    int64_t partial_data4;
    int64_t partial_data5;
    int64_t partial_data6;
    int64_t offset;
    int64_t sensitivity;
    int64_t press_comp;
    int64_t t_lin_64;

    t_lin_64 = (int64_t)t_lin;

    partial_data1 = t_lin_64 * t_lin_64;
    partial_data2 = partial_data1 / 64;
    partial_data3 = partial_data2 * t_lin_64 / 256;
    partial_data4 = dev->calib.par_p4 * partial_data3 / 32;
    partial_data5 = dev->calib.par_p3 * partial_data1 * 16;
    partial_data6 = dev->calib.par_p2 * t_lin_64 * (1 << 22);

    offset = dev->calib.par_p1 * ((int64_t)1 << 47) + partial_data4 + partial_data5 + partial_data6;
    partial_data2 = (dev->calib.par_p8 * partial_data3) / (1 << 5);
    partial_data4 = dev->calib.par_p7 * partial_data1 * (1 << 2);

    partial_data5 = (dev->calib.par_p6 - 16384) * t_lin_64 * (1 << 21);
    sensitivity = (dev->calib.par_p5 - 16384) * ((int64_t)1 << 46) + partial_data2 + partial_data4 + partial_data5;
    partial_data1 = sensitivity / (1 << 24) * pres_adc;

    partial_data2 = dev->calib.par_p10 * t_lin_64;
    partial_data3 = partial_data2 + dev->calib.par_p9 * (1 << 16);
    partial_data4 = partial_data3 * pres_adc / (1 << 13);
    partial_data5 = (pres_adc * partial_data4 / 10) / (1 << 9);
    partial_data5 = partial_data5 * 10;
    partial_data6 = pres_adc * pres_adc;

    partial_data2 = dev->calib.par_p11 * partial_data6 / (1 << 16);
    partial_data3 = partial_data2 * pres_adc / (1 << 7);
    partial_data4 = offset / 4 + partial_data1 + partial_data5 + partial_data3;

    press_comp = (partial_data4 / ((int64_t)1 << 40)) * 25;

    return (uint32_t)(press_comp / 100);
}

/* This internal API is used to calculate the humidity in integer */
static uint32_t calc_humidity(uint16_t hum_adc, int16_t comp_temperature, const struct bme69x_dev *dev)
{
    uint32_t hum_comp;
    int64_t hum_64 = hum_adc;
    int64_t t_comp = comp_temperature;
    int64_t t_fine = (t_comp * 256 - 128) / 5;
    int64_t var_H = t_fine - 76800UL;

    var_H =
        (((((hum_64 * 16384UL) - (dev->calib.par_h1 * 1048576UL) - (dev->calib.par_h2 * var_H)) + 16384UL) / 32768UL) *
         ((((((var_H * dev->calib.par_h4) / 1024UL) * ((var_H * dev->calib.par_h3) / 2048UL + 32768UL)) / 1024UL) +
           2097152ULL) * dev->calib.par_h5 + 8192UL) / 16384UL);

    var_H = var_H - (((((var_H / 32768UL) * (var_H / 32768UL)) / 128UL) * dev->calib.par_h6) / 16UL);

    if (var_H < 0)
    {
        var_H = 0;
    }

    if (var_H > 419430400)
    {
        var_H = 419430400;
    }

    hum_comp = (uint32_t)(var_H / 4096UL);

    return hum_comp;
}

/* This internal API is used to calculate the gas resistance */
static uint32_t calc_gas_resistance(uint16_t gas_res_adc, uint8_t gas_range)
{
    uint32_t calc_gas_res;
    uint32_t var1 = UINT32_C(262144) >> gas_range;
    int32_t var2 = (int32_t)gas_res_adc - INT32_C(512);

    var2 *= INT32_C(3);
    var2 = INT32_C(4096) + var2;

    /* multiplying 10000 then dividing then multiplying by 100 instead of multiplying by 1000000 to prevent overflow */
    calc_gas_res = (UINT32_C(10000) * var1) / (uint32_t)var2;
    calc_gas_res = calc_gas_res * 100;

    return calc_gas_res;
}

/* This internal API is used to calculate the heater resistance value using integer */
static uint8_t calc_res_heat(uint16_t temp, const struct bme69x_dev *dev)
{
    uint8_t heatr_res;
    int32_t var1;
    int32_t var2;
    int32_t var3;
    int32_t var4;
    int32_t var5;
    int32_t heatr_res_x100;

    if (temp > 400) /* Cap temperature */
    {
        temp = 400;
    }

    var1 = (((int32_t)dev->amb_temp * dev->calib.par_g3) / 1000U) * 256; /* par_g1 */
    var2 = (dev->calib.par_g1 + 784) * (((((dev->calib.par_g2 + 154009UL) * temp * 5) / 100) + 3276800ULL) / 10); /* par_g2,
                                                                                                                   * par_g3 */
    var3 = var1 + (var2 >> 1);
    var4 = (var3 / (dev->calib.res_heat_range + 4));
    var5 = (131 * dev->calib.res_heat_val) + 65536UL;
    heatr_res_x100 = (int32_t)(((var4 / var5) - 250) * 34);
    heatr_res = (uint8_t)((heatr_res_x100 + 50) / 100);

    return heatr_res;
}

#else

/* @brief This internal API is used to calculate the temperature value. */
static float calc_temperature(uint32_t temp_adc, const struct bme69x_dev *dev)
{
    int32_t do1, cf;
    double dtk1, dtk2, temp1, temp2;
    double calc_temp;

    do1 = (int32_t)dev->calib.par_t1 << 8;
    dtk1 = (double)dev->calib.par_t2 / (double)(1ULL << 30);
    dtk2 = (double)dev->calib.par_t3 / (double)(1ULL << 48);

    cf = temp_adc - do1;
    temp1 = (double)(cf * dtk1);
    temp2 = (double)cf * (double)cf * dtk2;

    calc_temp = temp1 + temp2;

    return (float)calc_temp;
}

/* @brief This internal API is used to calculate the pressure value. */
static float calc_pressure(uint32_t pres_adc, float comp_temperature, const struct bme69x_dev *dev)
{
    uint32_t o;
    double tk10, tk20, tk30;
    double s;
    double tk1s, tk2s, tk3s;
    double nls, tknls, nls3;
    double calc_pres, tmp1, tmp2, tmp3, tmp4;

    o = (uint32_t)dev->calib.par_p1 * (uint32_t)(1ULL << 3);
    tk10 = (double)dev->calib.par_p2 / (double)(1ULL << 6);
    tk20 = (double)dev->calib.par_p3 / (double)(1ULL << 8);
    tk30 = (double)dev->calib.par_p4 / (double)(1ULL << 15);

    s = ((double)dev->calib.par_p5 - (double)(1ULL << 14)) / (double)(1ULL << 20);
    tk1s = ((double)dev->calib.par_p6 - (double)(1ULL << 14)) / (double)(1ULL << 29);
    tk2s = (double)dev->calib.par_p7 / (double)(1ULL << 32);
    tk3s = (double)dev->calib.par_p8 / (double)(1ULL << 37);

    nls = (double)dev->calib.par_p9 / (double)(1ULL << 48);
    tknls = (double)dev->calib.par_p10 / (double)(1ULL << 48);

    /*
     * NLS3 = par_p11 / 2^65
     * 2^65 is exceeding the width of 'double' datatype and hence we splitted into two factors since A^(x+y) = A^x * A^y
     */
    nls3 = (double)dev->calib.par_p11 / ((double)(1ULL << 35) * (double)(1ULL << 30));

    tmp1 = (double)o + (tk10 * comp_temperature) + (tk20 * comp_temperature * comp_temperature) +
           (tk30 * comp_temperature * comp_temperature * comp_temperature);

    tmp2 = (double)pres_adc *
           ((double)s + (tk1s * comp_temperature) + (tk2s * comp_temperature * comp_temperature) +
            (tk3s * comp_temperature * comp_temperature * comp_temperature));

    tmp3 = (double)pres_adc * (double)pres_adc * (nls + (tknls * comp_temperature));
    tmp4 = (double)pres_adc * (double)pres_adc * (double)pres_adc * nls3;

    calc_pres = tmp1 + tmp2 + tmp3 + tmp4;

    return (float)calc_pres;
}

/* This internal API is used to calculate the humidity in integer */
static float calc_humidity(uint16_t hum_adc, float comp_temperature, const struct bme69x_dev *dev)
{
    double oh, tk10h, sh;
    double tk1sh, tk2sh, hlin2;
    double hoff, hsens;
    double temp_comp, calc_hum, hum_float_val;
    int32_t hum_int_val;

    temp_comp = (comp_temperature * 5120) - 76800;

    oh = (double)dev->calib.par_h1 * (double)(1ULL << 6);
    sh = (double)dev->calib.par_h5 / (double)(1ULL << 16);
    tk10h = (double)dev->calib.par_h2 / (double)(1ULL << 14);
    tk1sh = (double)dev->calib.par_h4 / (double)(1ULL << 26);
    tk2sh = (double)dev->calib.par_h3 / (double)(1ULL << 26);
    hlin2 = (double)dev->calib.par_h6 / (double)(1ULL << 19);

    hoff = (double)hum_adc - (oh + tk10h * temp_comp);
    hsens = hoff * sh * (1 + (tk1sh * temp_comp) + (tk1sh * tk2sh * temp_comp * temp_comp));
    hum_float_val = hsens * (1 - hlin2 * hsens);

    /* Avoid direct floating-point comparison by using integer scaling */
    hum_int_val = (int32_t)(hum_float_val * 1000.0f);

    if (hum_int_val >= 100000)
    {
        hum_float_val = 100.0;
    }
    else if (hum_int_val < 0)
    {
        hum_float_val = 0.0;
    }

    calc_hum = hum_float_val;

    return (float)calc_hum;
}

/* This internal API is used to calculate the gas resistance value for BME69x variant in float */
static float calc_gas_resistance(uint16_t gas_res_adc, uint8_t gas_range)
{
    float calc_gas_res;

    uint32_t var1 = UINT32_C(262144) >> gas_range;
    int32_t var2 = (int32_t)gas_res_adc - INT32_C(512);

    var2 *= INT32_C(3);
    var2 = INT32_C(4096) + var2;

    calc_gas_res = 1000000.0f * (float)var1 / (float)var2;

    return calc_gas_res;
}

/* This internal API is used to calculate the heater resistance value using float */
static uint8_t calc_res_heat(uint16_t temp, const struct bme69x_dev *dev)
{
    float var1;
    float var2;
    float var3;
    float var4;
    float var5;
    uint8_t res_heat;

    if (temp > 400) /* Cap temperature */
    {
        temp = 400;
    }

    var1 = (((float)dev->calib.par_g1 / (16.0f)) + 49.0f);
    var2 = ((((float)dev->calib.par_g2 / (32768.0f)) * (0.0005f)) + 0.00235f);
    var3 = ((float)dev->calib.par_g3 / (1024.0f));
    var4 = (var1 * (1.0f + (var2 * (float)temp)));
    var5 = (var4 + (var3 * (float)dev->amb_temp));
    res_heat =
        (uint8_t)(3.4f *
                  ((var5 * (4 / (4 + (float)dev->calib.res_heat_range)) *
                    (1 / (1 + ((float)dev->calib.res_heat_val * 0.002f)))) -
                   25));

    return res_heat;
}

#endif

/* This internal API is used to calculate the gas wait */
static uint8_t calc_gas_wait(uint16_t dur)
{
    uint8_t factor = 0;
    uint8_t durval;

    if (dur >= 0xfc0)
    {
        durval = 0xff; /* Max duration*/
    }
    else
    {
        while (dur > 0x3F)
        {
            dur = dur / 4;
            factor += 1;
        }

        durval = (uint8_t)(dur + (factor * 64));
    }

    return durval;
}

/* This internal API is used to read a single data of the sensor */
static int8_t read_field_data(uint8_t index, struct bme69x_data *data, struct bme69x_dev *dev)
{
    int8_t rslt = BME69X_OK;
    uint8_t buff[BME69X_LEN_FIELD] = { 0 };
    uint8_t gas_range;
    uint32_t adc_temp;
    uint32_t adc_pres;
    volatile uint16_t adc_hum;
    uint16_t adc_gas_res;
    uint8_t tries = 5;

    while ((tries) && (rslt == BME69X_OK))
    {
        rslt = bme69x_get_regs(((uint8_t)(BME69X_REG_FIELD0 + (index * BME69X_LEN_FIELD_OFFSET))),
                               buff,
                               (uint16_t)BME69X_LEN_FIELD,
                               dev);
        if (!data)
        {
            rslt = BME69X_E_NULL_PTR;
            break;
        }

        data->status = buff[0] & BME69X_NEW_DATA_MSK;
        data->gas_index = buff[0] & BME69X_GAS_INDEX_MSK;
        data->meas_index = buff[1];

        /* read the raw data from the sensor */
        adc_pres = (uint32_t)(((uint32_t)buff[2] << 16) | ((uint32_t)buff[3] << 8) | ((uint32_t)buff[4]));
        adc_temp = (uint32_t)(((uint32_t)buff[5] << 16) | ((uint32_t)buff[6] << 8) | ((uint32_t)buff[7]));
        adc_hum = (uint16_t)(((uint32_t)buff[8] << 8) | (uint32_t)buff[9]);
        adc_gas_res = ((uint16_t)buff[15] << 2) | ((uint16_t)buff[16] >> 6);

        gas_range = buff[16] & BME69X_GAS_RANGE_MSK;

        data->status |= buff[16] & BME69X_GASM_VALID_MSK;
        data->status |= buff[16] & BME69X_HEAT_STAB_MSK;

        if ((data->status & BME69X_NEW_DATA_MSK) && (rslt == BME69X_OK))
        {
            rslt = bme69x_get_regs(BME69X_REG_RES_HEAT0 + data->gas_index, &data->res_heat, 1, dev);
            if (rslt == BME69X_OK)
            {
                rslt = bme69x_get_regs(BME69X_REG_IDAC_HEAT0 + data->gas_index, &data->idac, 1, dev);
            }

            if (rslt == BME69X_OK)
            {
                rslt = bme69x_get_regs(BME69X_REG_GAS_WAIT0 + data->gas_index, &data->gas_wait, 1, dev);
            }

            if (rslt == BME69X_OK)
            {
#ifndef BME69X_USE_FPU
                data->temperature = calc_temperature(adc_temp, dev, &data->t_lin);
                data->pressure = calc_pressure(adc_pres, data->t_lin, dev);
#else
                data->temperature = calc_temperature(adc_temp, dev);
                data->pressure = calc_pressure(adc_pres, data->temperature, dev);
#endif
                data->humidity = calc_humidity(adc_hum, data->temperature, dev);
                data->gas_resistance = calc_gas_resistance(adc_gas_res, gas_range);

                break;
            }
        }

        if (rslt == BME69X_OK)
        {
            dev->delay_us(BME69X_PERIOD_POLL, dev->intf_ptr);
        }

        tries--;
    }

    return rslt;
}

/* This internal API is used to read all data fields of the sensor */
static int8_t read_all_field_data(struct bme69x_data * const data[], struct bme69x_dev *dev)
{
    int8_t rslt = BME69X_OK;
    uint8_t buff[BME69X_LEN_FIELD * 3] = { 0 };
    uint8_t gas_range;
    uint32_t adc_temp;
    uint32_t adc_pres;
    uint16_t adc_hum;
    uint16_t adc_gas_res;
    uint8_t off;
    uint8_t set_val[30] = { 0 }; /* idac, res_heat, gas_wait */
    uint8_t i;

    if (!data[0] && !data[1] && !data[2])
    {
        rslt = BME69X_E_NULL_PTR;
    }

    if (rslt == BME69X_OK)
    {
        rslt = bme69x_get_regs(BME69X_REG_FIELD0, buff, (uint32_t) BME69X_LEN_FIELD * 3, dev);
    }

    if (rslt == BME69X_OK)
    {
        rslt = bme69x_get_regs(BME69X_REG_IDAC_HEAT0, set_val, 30, dev);
    }

    for (i = 0; ((i < 3) && (rslt == BME69X_OK)); i++)
    {
        off = (uint8_t)(i * BME69X_LEN_FIELD);
        data[i]->status = buff[off] & BME69X_NEW_DATA_MSK;
        data[i]->gas_index = buff[off] & BME69X_GAS_INDEX_MSK;
        data[i]->meas_index = buff[off + 1];

        /* read the raw data from the sensor */
        adc_pres =
            (uint32_t)(((uint32_t) buff[off + 2] << 16) | ((uint32_t) buff[off + 3] << 8) | ((uint32_t) buff[off + 4]));
        adc_temp =
            (uint32_t)(((uint32_t) buff[off + 5] << 16) | ((uint32_t) buff[off + 6] << 8) | ((uint32_t) buff[off + 7]));
        adc_hum = (uint16_t) (((uint32_t) buff[off + 8] * 256) | (uint32_t) buff[off + 9]);
        adc_gas_res = ((uint16_t)buff[off + 15] << 2) | ((uint16_t)buff[off + 16] >> 6);
        gas_range = buff[off + 16] & BME69X_GAS_RANGE_MSK;

        data[i]->status |= buff[off + 16] & BME69X_GASM_VALID_MSK;
        data[i]->status |= buff[off + 16] & BME69X_HEAT_STAB_MSK;

        data[i]->idac = set_val[data[i]->gas_index];
        data[i]->res_heat = set_val[10 + data[i]->gas_index];
        data[i]->gas_wait = set_val[20 + data[i]->gas_index];

#ifndef BME69X_USE_FPU

        /*
         * Fixed point calculation needs t_lin for pressure calculation
         * t_lin is calculated during temperature calculation
         */
        data[i]->temperature = calc_temperature(adc_temp, dev, &data[i]->t_lin);
        data[i]->pressure = calc_pressure(adc_pres, data[i]->t_lin, dev);
#else
        data[i]->temperature = calc_temperature(adc_temp, dev);
        data[i]->pressure = calc_pressure(adc_pres, data[i]->temperature, dev);
#endif
        data[i]->humidity = calc_humidity(adc_hum, data[i]->temperature, dev);
        data[i]->gas_resistance = calc_gas_resistance(adc_gas_res, gas_range);
    }

    return rslt;
}

/* This internal API is used to switch between SPI memory pages */
static int8_t set_mem_page(uint8_t reg_addr, struct bme69x_dev *dev)
{
    int8_t rslt;
    uint8_t reg;
    uint8_t mem_page;

    /* Check for null pointers in the device structure*/
    rslt = null_ptr_check(dev);
    if (rslt == BME69X_OK)
    {
        if (reg_addr > 0x7f)
        {
            mem_page = BME69X_MEM_PAGE1;
        }
        else
        {
            mem_page = BME69X_MEM_PAGE0;
        }

        if (mem_page != dev->mem_page)
        {
            dev->mem_page = mem_page;
            dev->intf_rslt = dev->read(BME69X_REG_MEM_PAGE | BME69X_SPI_RD_MSK, &reg, 1, dev->intf_ptr);
            if (dev->intf_rslt != 0)
            {
                rslt = BME69X_E_COM_FAIL;
            }

            if (rslt == BME69X_OK)
            {
                reg = reg & (~BME69X_MEM_PAGE_MSK);
                reg = reg | (dev->mem_page & BME69X_MEM_PAGE_MSK);
                dev->intf_rslt = dev->write(BME69X_REG_MEM_PAGE & BME69X_SPI_WR_MSK, &reg, 1, dev->intf_ptr);
                if (dev->intf_rslt != 0)
                {
                    rslt = BME69X_E_COM_FAIL;
                }
            }
        }
    }

    return rslt;
}

/* This internal API is used to get the current SPI memory page */
static int8_t get_mem_page(struct bme69x_dev *dev)
{
    int8_t rslt;
    uint8_t reg;

    /* Check for null pointer in the device structure*/
    rslt = null_ptr_check(dev);
    if (rslt == BME69X_OK)
    {
        dev->intf_rslt = dev->read(BME69X_REG_MEM_PAGE | BME69X_SPI_RD_MSK, &reg, 1, dev->intf_ptr);
        if (dev->intf_rslt != 0)
        {
            rslt = BME69X_E_COM_FAIL;
        }
        else
        {
            dev->mem_page = reg & BME69X_MEM_PAGE_MSK;
        }
    }

    return rslt;
}

/* This internal API is used to limit the max value of a parameter */
static int8_t boundary_check(uint8_t *value, uint8_t max, struct bme69x_dev *dev)
{
    int8_t rslt;

    rslt = null_ptr_check(dev);
    if ((value != NULL) && (rslt == BME69X_OK))
    {
        /* Check if value is above maximum value */
        if (*value > max)
        {
            /* Auto correct the invalid value to maximum value */
            *value = max;
            dev->info_msg |= BME69X_I_PARAM_CORR;
        }
    }
    else
    {
        rslt = BME69X_E_NULL_PTR;
    }

    return rslt;
}

/* This internal API is used to check the bme69x_dev for null pointers */
static int8_t null_ptr_check(const struct bme69x_dev *dev)
{
    int8_t rslt = BME69X_OK;

    if ((dev == NULL) || (dev->read == NULL) || (dev->write == NULL) || (dev->delay_us == NULL))
    {
        /* Device structure pointer is not valid */
        rslt = BME69X_E_NULL_PTR;
    }

    return rslt;
}

/* This internal API is used to set heater configurations */
static int8_t set_conf(const struct bme69x_heatr_conf *conf, uint8_t op_mode, uint8_t *nb_conv, struct bme69x_dev *dev)
{
    int8_t rslt = BME69X_OK;
    uint8_t i;
    uint8_t shared_dur;
    uint8_t write_len = 0;
    uint8_t heater_dur_shared_addr = BME69X_REG_SHD_HEATR_DUR;
    uint8_t rh_reg_addr[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    uint8_t rh_reg_data[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    uint8_t gw_reg_addr[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    uint8_t gw_reg_data[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    switch (op_mode)
    {
        case BME69X_FORCED_MODE:
            rh_reg_addr[0] = BME69X_REG_RES_HEAT0;
            rh_reg_data[0] = calc_res_heat(conf->heatr_temp, dev);
            gw_reg_addr[0] = BME69X_REG_GAS_WAIT0;
            gw_reg_data[0] = calc_gas_wait(conf->heatr_dur);
            (*nb_conv) = 0;
            write_len = 1;
            break;
        case BME69X_SEQUENTIAL_MODE:
            if ((!conf->heatr_dur_prof) || (!conf->heatr_temp_prof))
            {
                rslt = BME69X_E_NULL_PTR;
                break;
            }

            for (i = 0; i < conf->profile_len; i++)
            {
                rh_reg_addr[i] = BME69X_REG_RES_HEAT0 + i;
                rh_reg_data[i] = calc_res_heat(conf->heatr_temp_prof[i], dev);
                gw_reg_addr[i] = BME69X_REG_GAS_WAIT0 + i;
                gw_reg_data[i] = calc_gas_wait(conf->heatr_dur_prof[i]);
            }

            (*nb_conv) = conf->profile_len;
            write_len = conf->profile_len;
            break;
        case BME69X_PARALLEL_MODE:
            if ((!conf->heatr_dur_prof) || (!conf->heatr_temp_prof))
            {
                rslt = BME69X_E_NULL_PTR;
                break;
            }

            if (conf->shared_heatr_dur == 0)
            {
                rslt = BME69X_W_DEFINE_SHD_HEATR_DUR;
            }

            for (i = 0; i < conf->profile_len; i++)
            {
                rh_reg_addr[i] = BME69X_REG_RES_HEAT0 + i;
                rh_reg_data[i] = calc_res_heat(conf->heatr_temp_prof[i], dev);
                gw_reg_addr[i] = BME69X_REG_GAS_WAIT0 + i;
                gw_reg_data[i] = (uint8_t) conf->heatr_dur_prof[i];
            }

            (*nb_conv) = conf->profile_len;
            write_len = conf->profile_len;
            shared_dur = calc_heatr_dur_shared(conf->shared_heatr_dur);
            if (rslt == BME69X_OK)
            {
                rslt = bme69x_set_regs(&heater_dur_shared_addr, &shared_dur, 1, dev);
            }

            break;
        default:
            rslt = BME69X_W_DEFINE_OP_MODE;
    }

    if (rslt == BME69X_OK)
    {
        rslt = bme69x_set_regs(rh_reg_addr, rh_reg_data, write_len, dev);
    }

    if (rslt == BME69X_OK)
    {
        rslt = bme69x_set_regs(gw_reg_addr, gw_reg_data, write_len, dev);
    }

    return rslt;
}

/* This internal API is used to calculate the register value for
 * shared heater duration */
static uint8_t calc_heatr_dur_shared(uint16_t dur)
{
    uint8_t factor = 0;
    uint8_t heatdurval;

    if (dur >= 0x783)
    {
        heatdurval = 0xff; /* Max duration */
    }
    else
    {
        /* Step size of 0.477ms */
        dur = (uint16_t)(((uint32_t)dur * 1000) / 477);
        while (dur > 0x3F)
        {
            dur = dur >> 2;
            factor += 1;
        }

        heatdurval = (uint8_t)(dur + (factor * 64));
    }

    return heatdurval;
}

/* This internal API is used sort the sensor data */
static void sort_sensor_data(uint8_t low_index, uint8_t high_index, struct bme69x_data *field[])
{
    int16_t meas_index1;
    int16_t meas_index2;

    meas_index1 = (int16_t)field[low_index]->meas_index;
    meas_index2 = (int16_t)field[high_index]->meas_index;
    if ((field[low_index]->status & BME69X_NEW_DATA_MSK) && (field[high_index]->status & BME69X_NEW_DATA_MSK))
    {
        int16_t diff = meas_index2 - meas_index1;
        if (((diff > -3) && (diff < 0)) || (diff > 2))
        {
            swap_fields(low_index, high_index, field);
        }
    }
    else if (field[high_index]->status & BME69X_NEW_DATA_MSK)
    {
        swap_fields(low_index, high_index, field);
    }

    /* Sorting field data
     *
     * The 3 fields are filled in a fixed order with data in an incrementing
     * 8-bit sub-measurement index which looks like
     * Field index | Sub-meas index
     *      0      |        0
     *      1      |        1
     *      2      |        2
     *      0      |        3
     *      1      |        4
     *      2      |        5
     *      ...
     *      0      |        252
     *      1      |        253
     *      2      |        254
     *      0      |        255
     *      1      |        0
     *      2      |        1
     *
     * The fields are sorted in a way so as to always deal with only a snapshot
     * of comparing 2 fields at a time. The order being
     * field0 & field1
     * field0 & field2
     * field1 & field2
     * Here the oldest data should be in field0 while the newest is in field2.
     * In the following documentation, field0's position would referred to as
     * the lowest and field2 as the highest.
     *
     * In order to sort we have to consider the following cases,
     *
     * Case A: No fields have new data
     *     Then do not sort, as this data has already been read.
     *
     * Case B: Higher field has new data
     *     Then the new field get's the lowest position.
     *
     * Case C: Both fields have new data
     *     We have to put the oldest sample in the lowest position. Since the
     *     sub-meas index contains in essence the age of the sample, we calculate
     *     the difference between the higher field and the lower field.
     *     Here we have 3 sub-cases,
     *     Case 1: Regular read without overwrite
     *         Field index | Sub-meas index
     *              0      |        3
     *              1      |        4
     *
     *         Field index | Sub-meas index
     *              0      |        3
     *              2      |        5
     *
     *         The difference is always <= 2. There is no need to swap as the
     *         oldest sample is already in the lowest position.
     *
     *     Case 2: Regular read with an overflow and without an overwrite
     *         Field index | Sub-meas index
     *              0      |        255
     *              1      |        0
     *
     *         Field index | Sub-meas index
     *              0      |        254
     *              2      |        0
     *
     *         The difference is always <= -3. There is no need to swap as the
     *         oldest sample is already in the lowest position.
     *
     *     Case 3: Regular read with overwrite
     *         Field index | Sub-meas index
     *              0      |        6
     *              1      |        4
     *
     *         Field index | Sub-meas index
     *              0      |        6
     *              2      |        5
     *
     *         The difference is always > -3. There is a need to swap as the
     *         oldest sample is not in the lowest position.
     *
     *     Case 4: Regular read with overwrite and overflow
     *         Field index | Sub-meas index
     *              0      |        0
     *              1      |        254
     *
     *         Field index | Sub-meas index
     *              0      |        0
     *              2      |        255
     *
     *         The difference is always > 2. There is a need to swap as the
     *         oldest sample is not in the lowest position.
     *
     * To summarize, we have to swap when
     *     - The higher field has new data and the lower field does not.
     *     - If both fields have new data, then the difference of sub-meas index
     *         between the higher field and the lower field creates the
     *         following condition for swapping.
     *         - (diff > -3) && (diff < 0), combination of cases 1, 2, and 3.
     *         - diff > 2, case 4.
     *
     *     Here the limits of -3 and 2 derive from the fact that there are 3 fields.
     *     These values decrease or increase respectively if the number of fields increases.
     */
}

/* This internal API is used sort the sensor data */
static void swap_fields(uint8_t index1, uint8_t index2, struct bme69x_data *field[])
{
    struct bme69x_data *temp;

    temp = field[index1];
    field[index1] = field[index2];
    field[index2] = temp;
}

/* This Function is to analyze the sensor data */
static int8_t analyze_sensor_data(const struct bme69x_data *data, uint8_t n_meas)
{
    int8_t rslt = BME69X_OK;
    uint8_t self_test_failed = 0, i;
    uint32_t cent_res = 0;

    if ((data[0].temperature < BME69X_MIN_TEMPERATURE) || (data[0].temperature > BME69X_MAX_TEMPERATURE))
    {
        self_test_failed++;
    }

    if ((data[0].pressure < BME69X_MIN_PRESSURE) || (data[0].pressure > BME69X_MAX_PRESSURE))
    {
        self_test_failed++;
    }

    if ((data[0].humidity < BME69X_MIN_HUMIDITY) || (data[0].humidity > BME69X_MAX_HUMIDITY))
    {
        self_test_failed++;
    }

    for (i = 0; i < n_meas; i++) /* Every gas measurement should be valid */
    {
        if (!(data[i].status & BME69X_GASM_VALID_MSK))
        {
            self_test_failed++;
        }
    }

    if (n_meas >= 6)
    {
        cent_res = (uint32_t)((5 * (data[3].gas_resistance + data[5].gas_resistance)) / (2 * data[4].gas_resistance));
    }

    if (cent_res < 6)
    {
        self_test_failed++;
    }

    if (self_test_failed)
    {
        rslt = BME69X_E_SELF_TEST;
    }

    return rslt;
}

/* This internal API is used to read the calibration coefficients */
static int8_t get_calib_data(struct bme69x_dev *dev)
{
    int8_t rslt;
    uint8_t coeff_array[BME69X_LEN_COEFF_ALL];

    rslt = bme69x_get_regs(BME69X_REG_COEFF1, coeff_array, BME69X_LEN_COEFF1, dev);
    if (rslt == BME69X_OK)
    {
        rslt = bme69x_get_regs(BME69X_REG_COEFF2, &coeff_array[BME69X_LEN_COEFF1], BME69X_LEN_COEFF2, dev);
    }

    if (rslt == BME69X_OK)
    {
        rslt = bme69x_get_regs(BME69X_REG_COEFF3,
                               &coeff_array[BME69X_LEN_COEFF1 + BME69X_LEN_COEFF2],
                               BME69X_LEN_COEFF3,
                               dev);
    }

    if (rslt == BME69X_OK)
    {
        /* Temperature related coefficients */
        dev->calib.par_t1 =
            (uint16_t)(BME69X_CONCAT_BYTES(coeff_array[BME69X_IDX_DO_C_MSB], coeff_array[BME69X_IDX_DO_C_LSB]));
        dev->calib.par_t2 =
            (uint16_t)(BME69X_CONCAT_BYTES(coeff_array[BME69X_IDX_DTK1_C_MSB], coeff_array[BME69X_IDX_DTK1_C_LSB]));
        dev->calib.par_t3 = (int8_t)(coeff_array[BME69X_IDX_DTK2_C]);

        /* Pressure related coefficients */
        dev->calib.par_p5 =
            (int16_t)(BME69X_CONCAT_BYTES(coeff_array[BME69X_IDX_S_C_MSB], coeff_array[BME69X_IDX_S_C_LSB]));
        dev->calib.par_p6 =
            (int16_t)(BME69X_CONCAT_BYTES(coeff_array[BME69X_IDX_TK1S_C_MSB], coeff_array[BME69X_IDX_TK1S_C_LSB]));
        dev->calib.par_p7 = (int8_t)coeff_array[BME69X_IDX_TK2S_C];
        dev->calib.par_p8 = (int8_t)coeff_array[BME69X_IDX_TK3S_C];

        dev->calib.par_p1 =
            (int16_t)(BME69X_CONCAT_BYTES(coeff_array[BME69X_IDX_O_C_MSB], coeff_array[BME69X_IDX_O_C_LSB]));
        dev->calib.par_p2 =
            (uint16_t)(BME69X_CONCAT_BYTES(coeff_array[BME69X_IDX_TK10_C_MSB], coeff_array[BME69X_IDX_TK10_C_LSB]));
        dev->calib.par_p3 = (int8_t)(coeff_array[BME69X_IDX_TK20_C]);
        dev->calib.par_p4 = (int8_t)(coeff_array[BME69X_IDX_TK30_C]);

        dev->calib.par_p9 =
            (int16_t)(BME69X_CONCAT_BYTES(coeff_array[BME69X_IDX_NLS_C_MSB], coeff_array[BME69X_IDX_NLS_C_LSB]));
        dev->calib.par_p10 = (int8_t)(coeff_array[BME69X_IDX_TKNLS_C]);
        dev->calib.par_p11 = (int8_t)(coeff_array[BME69X_IDX_NLS3_C]);

        /* Humidity related coefficients */
        dev->calib.par_h5 =
            (int16_t)(((int16_t)coeff_array[BME69X_IDX_S_H_MSB] << 4) | (coeff_array[BME69X_IDX_S_H_LSB] >> 4));

        if (dev->calib.par_h5 > 2047)
        {
            /* Convert to negative value */
            dev->calib.par_h5 = (int16_t)(dev->calib.par_h5 - 4096);
        }

        dev->calib.par_h1 =
            (int16_t)(((int16_t)coeff_array[BME69X_IDX_O_H_MSB] << 4) | (coeff_array[BME69X_IDX_O_H_LSB] & 0x0F));

        /* Check if the value is above 2047 */
        if (dev->calib.par_h1 > 2047)
        {
            /* Convert to negative value */
            dev->calib.par_h1 = (int16_t)(dev->calib.par_h1 - 4096);
        }

        dev->calib.par_h2 = (int8_t)coeff_array[BME69X_IDX_TK10H_C];
        dev->calib.par_h4 = (int8_t)coeff_array[BME69X_IDX_par_h4];
        dev->calib.par_h3 = (uint8_t)coeff_array[BME69X_IDX_par_h3];
        dev->calib.par_h6 = (uint8_t)coeff_array[BME69X_IDX_HLIN2_C];

        /* Gas heater related coefficients */
        dev->calib.par_g1 = (int8_t)coeff_array[BME69X_IDX_RO_C];
        dev->calib.par_g2 =
            (int16_t)(BME69X_CONCAT_BYTES(coeff_array[BME69X_IDX_TKR_C_MSB], coeff_array[BME69X_IDX_TKR_C_LSB]));
        dev->calib.par_g3 = (int8_t)coeff_array[BME69X_IDX_T_AMB_COMP];

        /* Other coefficients */
        dev->calib.res_heat_range = ((coeff_array[BME69X_IDX_RES_HEAT_RANGE] & BME69X_RHRANGE_MSK) >> 4);
        dev->calib.res_heat_val = (int8_t)coeff_array[BME69X_IDX_RES_HEAT_VAL];
        dev->calib.range_sw_err = ((int8_t)(coeff_array[BME69X_IDX_RANGE_SW_ERR] & BME69X_RSERROR_MSK)) / 16;
    }

    return rslt;
}

/* This internal API is used to read variant ID information from the register */
static int8_t read_variant_id(struct bme69x_dev *dev)
{
    int8_t rslt;
    uint8_t reg_data = 0;

    /* Read variant ID information register */
    rslt = bme69x_get_regs(BME69X_REG_VARIANT_ID, &reg_data, 1, dev);

    if (rslt == BME69X_OK)
    {
        dev->variant_id = reg_data;
    }

    return rslt;
}
