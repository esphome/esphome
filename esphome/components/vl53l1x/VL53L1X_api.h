/**
 *
 * Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

/**
 * @file  vl53l1x_api.h
 * @brief Functions definition
 */

#pragma once

#include "vl53l1_platform.h"

namespace esphome {
namespace vl53l1x {
namespace st_vl53l1x_uld {

const uint8_t VL53L1X_IMPLEMENTATION_VER_MAJOR = 3;
const uint8_t VL53L1X_IMPLEMENTATION_VER_MINOR = 5;
const uint8_t VL53L1X_IMPLEMENTATION_VER_SUB = 5;
const uint32_t VL53L1X_IMPLEMENTATION_VER_REVISION = 0000;

typedef uint8_t VL53L1X_ERROR;

const uint8_t VL53L1X_ERROR_NONE = ((uint8_t) 0U);
const uint8_t VL53L1X_ERROR_XTALK_FAILED = ((uint8_t) 253U);
const uint8_t VL53L1X_ERROR_INVALID_ARGUMENT = ((uint8_t) 254U);
const uint8_t VL53L1X_ERROR_TIMEOUT = ((uint8_t) 255U);

const uint16_t SOFT_RESET = 0x0000;
const uint16_t VL53L1_I2C__DEVICE_ADDRESS = 0x0001;
const uint16_t VL53L1_VHV_CONFIG__TIMEOUT_MACROP_LOOP_BOUND = 0x0008;
const uint16_t ALGO__CROSSTALK_COMPENSATION_PLANE_OFFSET_KCPS = 0x0016;
const uint16_t ALGO__CROSSTALK_COMPENSATION_X_PLANE_GRADIENT_KCPS = 0x0018;
const uint16_t ALGO__CROSSTALK_COMPENSATION_Y_PLANE_GRADIENT_KCPS = 0x001A;
const uint16_t ALGO__PART_TO_PART_RANGE_OFFSET_MM = 0x001E;
const uint16_t MM_CONFIG__INNER_OFFSET_MM = 0x0020;
const uint16_t MM_CONFIG__OUTER_OFFSET_MM = 0x0022;
const uint16_t GPIO_HV_MUX__CTRL = 0x0030;
const uint16_t GPIO__TIO_HV_STATUS = 0x0031;
const uint16_t SYSTEM__INTERRUPT_CONFIG_GPIO = 0x0046;
const uint16_t PHASECAL_CONFIG__TIMEOUT_MACROP = 0x004B;
const uint16_t RANGE_CONFIG__TIMEOUT_MACROP_A_HI = 0x005E;
const uint16_t RANGE_CONFIG__VCSEL_PERIOD_A = 0x0060;
const uint16_t RANGE_CONFIG__VCSEL_PERIOD_B = 0x0063;
const uint16_t RANGE_CONFIG__TIMEOUT_MACROP_B_HI = 0x0061;
const uint16_t RANGE_CONFIG__TIMEOUT_MACROP_B_LO = 0x0062;
const uint16_t RANGE_CONFIG__SIGMA_THRESH = 0x0064;
const uint16_t RANGE_CONFIG__MIN_COUNT_RATE_RTN_LIMIT_MCPS = 0x0066;
const uint16_t RANGE_CONFIG__VALID_PHASE_HIGH = 0x0069;
const uint16_t VL53L1_SYSTEM__INTERMEASUREMENT_PERIOD = 0x006C;
const uint16_t SYSTEM__THRESH_HIGH = 0x0072;
const uint16_t SYSTEM__THRESH_LOW = 0x0074;
const uint16_t SD_CONFIG__WOI_SD0 = 0x0078;
const uint16_t SD_CONFIG__INITIAL_PHASE_SD0 = 0x007A;
const uint16_t ROI_CONFIG__USER_ROI_CENTRE_SPAD = 0x007F;
const uint16_t ROI_CONFIG__USER_ROI_REQUESTED_GLOBAL_XY_SIZE = 0x0080;
const uint16_t SYSTEM__SEQUENCE_CONFIG = 0x0081;
const uint16_t VL53L1_SYSTEM__GROUPED_PARAMETER_HOLD = 0x0082;
const uint16_t SYSTEM__INTERRUPT_CLEAR = 0x0086;
const uint16_t SYSTEM__MODE_START = 0x0087;
const uint16_t VL53L1_RESULT__RANGE_STATUS = 0x0089;
const uint16_t VL53L1_RESULT__DSS_ACTUAL_EFFECTIVE_SPADS_SD0 = 0x008C;
const uint16_t RESULT__AMBIENT_COUNT_RATE_MCPS_SD = 0x0090;
const uint16_t VL53L1_RESULT__FINAL_CROSSTALK_CORRECTED_RANGE_MM_SD0 = 0x0096;
const uint16_t VL53L1_RESULT__PEAK_SIGNAL_COUNT_RATE_CROSSTALK_CORRECTED_MCPS_SD0 = 0x0098;
const uint16_t VL53L1_RESULT__OSC_CALIBRATE_VAL = 0x00DE;
const uint16_t VL53L1_FIRMWARE__SYSTEM_STATUS = 0x00E5;
const uint16_t VL53L1_IDENTIFICATION__MODEL_ID = 0x010F;
const uint16_t VL53L1_ROI_CONFIG__MODE_ROI_CENTRE_SPAD = 0x013E;

/**
 * @brief This function sets the sensor I2C address used in case multiple devices application, default address 0x52
 */
VL53L1X_ERROR VL53L1X_SetI2CAddress(uint16_t, uint8_t new_address);

/**
 * @brief This function loads the 135 bytes default values to initialize the sensor.
 * @param dev Device address
 * @return 0:success, != 0:failed
 */
VL53L1X_ERROR VL53L1X_SensorInit(uint16_t dev);

/**
 * @brief This function clears the interrupt, to be called after a ranging data reading
 * to arm the interrupt for the next data ready event.
 */
VL53L1X_ERROR VL53L1X_ClearInterrupt(uint16_t dev);

/**
 * @brief This function starts the ranging distance operation\n
 * The ranging operation is continuous. The clear interrupt has to be done after each get data to allow the interrupt to
 * raise when the next data is ready\n 1=active high (default), 0=active low, use SetInterruptPolarity() to change the
 * interrupt polarity if required.
 */
VL53L1X_ERROR VL53L1X_StartRanging(uint16_t dev);

/**
 * @brief This function stops the ranging.
 */
VL53L1X_ERROR VL53L1X_StopRanging(uint16_t dev);

/**
 * @brief This function checks if the new ranging data is available by polling the dedicated register.
 * @param : is_data_ready==0 -> not ready; is_data_ready==1 -> ready
 */
VL53L1X_ERROR VL53L1X_CheckForDataReady(uint16_t dev, uint8_t *is_data_ready);

/**
 * @brief This function programs the timing budget in ms.
 * Predefined values = 15, 20, 33, 50, 100(default), 200, 500.
 */
VL53L1X_ERROR VL53L1X_SetTimingBudgetInMs(uint16_t dev, uint16_t timing_budget_in_ms);

/**
 * @brief This function programs the distance mode (1=short, 2=long(default)).
 * Short mode max distance is limited to 1.3 m but better ambient immunity.\n
 * Long mode can range up to 4 m in the dark with 200 ms timing budget.
 */
VL53L1X_ERROR VL53L1X_SetDistanceMode(uint16_t dev, uint16_t distance_mode);

/**
 * @brief This function returns the current distance mode (1=short, 2=long).
 */
VL53L1X_ERROR VL53L1X_GetDistanceMode(uint16_t dev, uint16_t *distance_mode);

/**
 * @brief This function programs the Intermeasurement period in ms\n
 * Intermeasurement period must be >/= timing budget. This condition is not checked by the API,
 * the customer has the duty to check the condition. Default = 100 ms
 */
VL53L1X_ERROR VL53L1X_SetInterMeasurementInMs(uint16_t dev, uint32_t intermeasurement_in_ms);

/**
 * @brief This function returns the boot state of the device (1:booted, 0:not booted)
 */
VL53L1X_ERROR VL53L1X_BootState(uint16_t dev, uint8_t *state);

/**
 * @brief This function returns the distance measured by the sensor in mm
 */
VL53L1X_ERROR VL53L1X_GetDistance(uint16_t dev, uint16_t *distance);

/**
 * @brief This function returns the ranging status error \n
 * (0:no error, 1:sigma failed, 2:signal failed, ..., 7:wrap-around)
 */
VL53L1X_ERROR VL53L1X_GetRangeStatus(uint16_t dev, uint8_t *range_status);

/**
 * @brief This function programs the offset correction in mm
 * @param offset_value:the offset correction value to program in mm
 */
VL53L1X_ERROR VL53L1X_SetOffset(uint16_t dev, int16_t offset_value);

/**
 * @brief This function programs the xtalk correction value in cps (Count Per Second).\n
 * This is the number of photons reflected back from the cover glass in cps.
 */
VL53L1X_ERROR VL53L1X_SetXtalk(uint16_t dev, uint16_t xtalk_value);

/**
 * @brief This function programs the threshold detection mode\n
 * Example:\n
 * VL53L1X_SetDistanceThreshold(dev,100,300,0,1): Below 100 \n
 * VL53L1X_SetDistanceThreshold(dev,100,300,1,1): Above 300 \n
 * VL53L1X_SetDistanceThreshold(dev,100,300,2,1): Out of window \n
 * VL53L1X_SetDistanceThreshold(dev,100,300,3,1): In window \n
 * @param   dev : device address
 * @param   ThreshLow(in mm) : the threshold under which one the device raises an interrupt if Window = 0
 * @param   ThreshHigh(in mm) :  the threshold above which one the device raises an interrupt if Window = 1
 * @param   Window detection mode : 0=below, 1=above, 2=out, 3=in
 * @param   IntOnNoTarget = 0 (No longer used - just use 0)
 */
VL53L1X_ERROR VL53L1X_SetDistanceThreshold(uint16_t dev, uint16_t ThreshLow, uint16_t ThreshHigh, uint8_t Window,
                                           uint8_t IntOnNoTarget);

/**
 * @brief This function programs the ROI (Region of Interest)\n
 * The ROI position is centered, only the ROI size can be reprogrammed.\n
 * The smallest acceptable ROI size = 4\n
 * @param x:ROI Width; y=ROI Height
 */
VL53L1X_ERROR VL53L1X_SetROI(uint16_t dev, uint16_t x, uint16_t y);

/**
 *@brief This function programs the new user ROI center, please to be aware that there is no check in this function.
 *if the ROI center vs ROI size is out of border the ranging function return error #13
 */
VL53L1X_ERROR VL53L1X_SetROICenter(uint16_t dev, uint8_t roi_center);

/**
 * @brief This function programs a new signal threshold in kcps (default=1024 kcps\n
 */
VL53L1X_ERROR VL53L1X_SetSignalThreshold(uint16_t dev, uint16_t signal);

/**
 * @brief This function programs a new sigma threshold in mm (default=15 mm)
 */
VL53L1X_ERROR VL53L1X_SetSigmaThreshold(uint16_t dev, uint16_t sigma);

/**
 * @brief This function performs the temperature calibration.
 * It is recommended to call this function any time the temperature might have changed by more than 8 deg C
 * without sensor ranging activity for an extended period.
 */
VL53L1X_ERROR VL53L1X_StartTemperatureUpdate(uint16_t dev);

}  // namespace st_vl53l1x_uld
}  // namespace vl53l1x
}  // namespace esphome
