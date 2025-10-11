/**
 * This file is adapted from STMicroelectronics Ultra Light Driver for the
 * VL53L1x sensor series.
 * The original LICENSE is included below.
 */
/*****************************************************************************
 *
 * Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.
 *
 * This software component is provided to you as part of a software package and
 * applicable license terms are in the  Package_license file. If you received this
 * software component outside of a package or without applicable license terms,
 * the terms of the BSD OPEN SOURCE SLA0103 license shall apply.
 * You may obtain a copy of the BSD OPEN SOURCE SLA0103 at:
 * https://www.st.com/SLA0103
 *****************************************************************************/

#include "driver.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/helpers.h"
#include <cstring>

namespace esphome {
namespace vl53l1x {
namespace driver {

const uint8_t VL51L1X_DEFAULT_CONFIGURATION[] = {
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
             init() call, put 0x40 in location 0x87 */
};

static const uint8_t STATUS_RTN[24] = {255, 255, 255, 5,   2,   4,   1,  7, 3,   0,   255, 255,
                                       9,   13,  255, 255, 255, 255, 10, 6, 255, 255, 11,  12};

int8_t WriteMulti(i2c::I2CDevice *dev, uint16_t index, uint8_t *pdata, uint32_t count) {
  uint8_t status = VL53L1X_ERROR_TIMEOUT;
  uint8_t err = 0;
  if ((err = dev->write_register16(index, pdata, count)) == 0) {
    status = VL53L1X_ERROR_NONE;
  }
  return status;
}

int8_t ReadMulti(i2c::I2CDevice *dev, uint16_t index, uint8_t *pdata, uint32_t count) {
  uint8_t status = VL53L1X_ERROR_TIMEOUT;
  uint8_t err = 0;
  if ((err = dev->read_register16(index, pdata, count)) == 0) {
    status = VL53L1X_ERROR_NONE;
  }

  return status;
}

int8_t WrByte(i2c::I2CDevice *dev, uint16_t index, uint8_t data) {
  return WriteMulti(dev, index, reinterpret_cast<uint8_t *>(&data), sizeof(data));
}

int8_t WrWord(i2c::I2CDevice *dev, uint16_t index, uint16_t data) {
  data = byteswap(data);
  return WriteMulti(dev, index, reinterpret_cast<uint8_t *>(&data), sizeof(data));
}

int8_t WrDWord(i2c::I2CDevice *dev, uint16_t index, uint32_t data) {
  data = byteswap(data);
  return WriteMulti(dev, index, reinterpret_cast<uint8_t *>(&data), sizeof(data));
}

int8_t RdByte(i2c::I2CDevice *dev, uint16_t index, uint8_t *data) {
  return ReadMulti(dev, index, reinterpret_cast<uint8_t *>(data), sizeof(*data));
}

int8_t RdWord(i2c::I2CDevice *dev, uint16_t index, uint16_t *data) {
  int8_t status = VL53L1X_ERROR_NONE;
  uint16_t tmp_data = 0;
  if ((status = ReadMulti(dev, index, reinterpret_cast<uint8_t *>(&tmp_data), sizeof(tmp_data))) ==
      VL53L1X_ERROR_NONE) {
    *data = byteswap(tmp_data);
  }
  return status;
}

int8_t RdDWord(i2c::I2CDevice *dev, uint16_t index, uint32_t *data) {
  int8_t status = VL53L1X_ERROR_NONE;
  uint32_t tmp_data = 0;
  if ((status = ReadMulti(dev, index, reinterpret_cast<uint8_t *>(&tmp_data), sizeof(tmp_data))) ==
      VL53L1X_ERROR_NONE) {
    *data = byteswap(tmp_data);
  }
  return status;
}

int8_t WaitMs(i2c::I2CDevice *dev, int32_t wait_ms) {
  ::esphome::delay(wait_ms);
  return VL53L1X_ERROR_NONE;
}

VL53L1X_ERROR SetI2CAddress(i2c::I2CDevice *dev, uint8_t new_address) {
  VL53L1X_ERROR status = 0;

  status |= WrByte(dev, I2C_DEVICE_ADDRESS, new_address >> 1);
  return status;
}

VL53L1X_ERROR SensorInit(i2c::I2CDevice *dev) {
  VL53L1X_ERROR status = 0;
  uint8_t addr = 0x00, tmp = 0;
  uint16_t timeout_counter = 0;

  for (addr = 0x2D; addr <= 0x87; addr++) {
    status |= WrByte(dev, addr, VL51L1X_DEFAULT_CONFIGURATION[addr - 0x2D]);
  }
  status |= StartRanging(dev);
  while (tmp == 0) {
    status = CheckForDataReady(dev, &tmp);
    timeout_counter++;
    if (timeout_counter >= 1000) {
      status = (uint8_t) VL53L1X_ERROR_TIMEOUT;
      return status;
    }
    status = WaitMs(dev, 1);
  }
  status |= ClearInterrupt(dev);
  status |= StopRanging(dev);
  status |= WrByte(dev, VHV_CONFIG_TIMEOUT_MACROP_LOOP_BOUND, 0x09); /* two bounds VHV */
  status |= WrByte(dev, 0x0B, 0);                                    /* start VHV from the previous temperature */
  return status;
}

VL53L1X_ERROR ClearInterrupt(i2c::I2CDevice *dev) {
  VL53L1X_ERROR status = 0;

  status |= WrByte(dev, SYSTEM_INTERRUPT_CLEAR, 0x01);
  return status;
}

VL53L1X_ERROR GetInterruptPolarity(i2c::I2CDevice *dev, uint8_t *interrupt_polarity) {
  uint8_t temp = 0;
  VL53L1X_ERROR status = 0;

  status |= RdByte(dev, GPIO_HV_MUX_CTRL, &temp);
  temp = temp & 0x10;
  *interrupt_polarity = !(temp >> 4);
  return status;
}

VL53L1X_ERROR StartRanging(i2c::I2CDevice *dev) {
  VL53L1X_ERROR status = 0;

  status |= WrByte(dev, SYSTEM_MODE_START, 0x40); /* Enable VL53L1X */
  return status;
}

VL53L1X_ERROR StopRanging(i2c::I2CDevice *dev) {
  VL53L1X_ERROR status = 0;

  status |= WrByte(dev, SYSTEM_MODE_START, 0x00); /* Disable VL53L1X */
  return status;
}

VL53L1X_ERROR CheckForDataReady(i2c::I2CDevice *dev, uint8_t *is_data_ready) {
  uint8_t temp;
  uint8_t interrupt_polarity;
  VL53L1X_ERROR status = 0;

  status |= GetInterruptPolarity(dev, &interrupt_polarity);
  status |= RdByte(dev, GPIO_TIO_HV_STATUS, &temp);
  /* Read in the register to check if a new value is available */
  if (status == 0) {
    if ((temp & 1) == interrupt_polarity) {
      *is_data_ready = 1;
    } else {
      *is_data_ready = 0;
    }
  }
  return status;
}

VL53L1X_ERROR SetTimingBudgetInMs(i2c::I2CDevice *dev, uint16_t timing_budget_in_ms) {
  uint16_t distance_mode;
  VL53L1X_ERROR status = 0;

  status |= GetDistanceMode(dev, &distance_mode);
  if (distance_mode == 0) {
    return 1;
  } else if (distance_mode == 1) { /* Short DistanceMode */
    switch (timing_budget_in_ms) {
      case 15: /* only available in short distance mode */
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x01D);
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x0027);
        break;
      case 20:
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x0051);
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x006E);
        break;
      case 33:
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x00D6);
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x006E);
        break;
      case 50:
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x1AE);
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x01E8);
        break;
      case 100:
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x02E1);
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x0388);
        break;
      case 200:
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x03E1);
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x0496);
        break;
      case 500:
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x0591);
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x05C1);
        break;
      default:
        status = 1;
        break;
    }
  } else {
    switch (timing_budget_in_ms) {
      case 20:
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x001E);
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x0022);
        break;
      case 33:
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x0060);
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x006E);
        break;
      case 50:
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x00AD);
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x00C6);
        break;
      case 100:
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x01CC);
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x01EA);
        break;
      case 200:
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x02D9);
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x02F8);
        break;
      case 500:
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x048F);
        WrWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x04A4);
        break;
      default:
        status = 1;
        break;
    }
  }
  return status;
}

VL53L1X_ERROR GetTimingBudgetInMs(i2c::I2CDevice *dev, uint16_t *timing_budget) {
  uint16_t temp;
  VL53L1X_ERROR status = 0;

  status |= RdWord(dev, RANGE_CONFIG_TIMEOUT_MACROP_A_HI, &temp);
  switch (temp) {
    case 0x001D:
      *timing_budget = 15;
      break;
    case 0x0051:
    case 0x001E:
      *timing_budget = 20;
      break;
    case 0x00D6:
    case 0x0060:
      *timing_budget = 33;
      break;
    case 0x1AE:
    case 0x00AD:
      *timing_budget = 50;
      break;
    case 0x02E1:
    case 0x01CC:
      *timing_budget = 100;
      break;
    case 0x03E1:
    case 0x02D9:
      *timing_budget = 200;
      break;
    case 0x0591:
    case 0x048F:
      *timing_budget = 500;
      break;
    default:
      status = 1;
      *timing_budget = 0;
  }
  return status;
}

VL53L1X_ERROR SetDistanceMode(i2c::I2CDevice *dev, uint16_t distance_mode) {
  uint16_t timing_budget;
  VL53L1X_ERROR status = 0;

  status |= GetTimingBudgetInMs(dev, &timing_budget);
  if (status != 0)
    return 1;
  switch (distance_mode) {
    case 1:
      status = WrByte(dev, PHASECAL_CONFIG_TIMEOUT_MACROP, 0x14);
      status = WrByte(dev, RANGE_CONFIG_VCSEL_PERIOD_A, 0x07);
      status = WrByte(dev, RANGE_CONFIG_VCSEL_PERIOD_B, 0x05);
      status = WrByte(dev, RANGE_CONFIG_VALID_PHASE_HIGH, 0x38);
      status = WrWord(dev, SD_CONFIG_WOI_SD0, 0x0705);
      status = WrWord(dev, SD_CONFIG_INITIAL_PHASE_SD0, 0x0606);
      break;
    case 2:
      status = WrByte(dev, PHASECAL_CONFIG_TIMEOUT_MACROP, 0x0A);
      status = WrByte(dev, RANGE_CONFIG_VCSEL_PERIOD_A, 0x0F);
      status = WrByte(dev, RANGE_CONFIG_VCSEL_PERIOD_B, 0x0D);
      status = WrByte(dev, RANGE_CONFIG_VALID_PHASE_HIGH, 0xB8);
      status = WrWord(dev, SD_CONFIG_WOI_SD0, 0x0F0D);
      status = WrWord(dev, SD_CONFIG_INITIAL_PHASE_SD0, 0x0E0E);
      break;
    default:
      status = 1;
      break;
  }

  if (status == 0)
    status |= SetTimingBudgetInMs(dev, timing_budget);
  return status;
}

VL53L1X_ERROR GetDistanceMode(i2c::I2CDevice *dev, uint16_t *distance_mode) {
  uint8_t temp, status = 0;

  status |= RdByte(dev, PHASECAL_CONFIG_TIMEOUT_MACROP, &temp);
  if (temp == 0x14)
    *distance_mode = 1;
  if (temp == 0x0A)
    *distance_mode = 2;
  return status;
}

VL53L1X_ERROR SetInterMeasurementInMs(i2c::I2CDevice *dev, uint32_t intermeasurement_in_ms) {
  uint16_t clock_pll = 0;
  VL53L1X_ERROR status = 0;

  status |= RdWord(dev, RESULT_OSC_CALIBRATE_VAL, &clock_pll);
  clock_pll = clock_pll & 0x3FF;
  WrDWord(dev, SYSTEM_INTERMEASUREMENT_PERIOD, (uint32_t) (clock_pll * intermeasurement_in_ms * 1.075));
  return status;
}

VL53L1X_ERROR BootState(i2c::I2CDevice *dev, uint8_t *state) {
  VL53L1X_ERROR status = 0;
  uint8_t tmp = 0;

  status |= RdByte(dev, FIRMWARE_SYSTEM_STATUS, &tmp);
  *state = tmp;
  return status;
}
VL53L1X_ERROR GetDistance(i2c::I2CDevice *dev, uint16_t *distance) {
  VL53L1X_ERROR status = 0;
  uint16_t tmp;

  status |= (RdWord(dev, RESULT_FINAL_CROSSTALK_CORRECTED_RANGE_MM_SD0, &tmp));
  *distance = tmp;
  return status;
}

VL53L1X_ERROR GetRangeStatus(i2c::I2CDevice *dev, uint8_t *range_status) {
  VL53L1X_ERROR status = 0;
  uint8_t temp = 0;

  *range_status = 255;
  status |= RdByte(dev, RESULT_RANGE_STATUS, &temp);
  temp = temp & 0x1F;
  if (temp < 24)
    *range_status = STATUS_RTN[temp];
  return status;
}

VL53L1X_ERROR SetOffset(i2c::I2CDevice *dev, int16_t offset_value) {
  VL53L1X_ERROR status = 0;
  int16_t temp = 0;

  temp = (offset_value * 4);
  status |= WrWord(dev, ALGO_PART_TO_PART_RANGE_OFFSET_MM, (uint16_t) temp);
  status |= WrWord(dev, MM_CONFIG_INNER_OFFSET_MM, 0x0);
  status |= WrWord(dev, MM_CONFIG_OUTER_OFFSET_MM, 0x0);
  return status;
}

VL53L1X_ERROR SetXtalk(i2c::I2CDevice *dev, uint16_t xtalk_value) {
  /* XTalkValue in count per second to avoid float type */
  VL53L1X_ERROR status = 0;

  status |= WrWord(dev, ALGO_CROSSTALK_COMPENSATION_X_PLANE_GRADIENT_KCPS, 0x0000);
  status |= WrWord(dev, ALGO_CROSSTALK_COMPENSATION_Y_PLANE_GRADIENT_KCPS, 0x0000);
  status |= WrWord(dev, ALGO_CROSSTALK_COMPENSATION_PLANE_OFFSET_KCPS,
                   (xtalk_value << 9) / 1000); /* * << 9 (7.9 format) and /1000 to convert cps to kpcs */
  return status;
}

VL53L1X_ERROR SetDistanceThreshold(i2c::I2CDevice *dev, uint16_t threshold_low, uint16_t threshold_high,
                                   uint8_t window) {
  VL53L1X_ERROR status = 0;
  uint8_t temp = 0;

  status |= RdByte(dev, SYSTEM_INTERRUPT_CONFIG_GPIO, &temp);
  temp = temp & (~0x6F);
  temp = temp | window;

  status = WrByte(dev, SYSTEM_INTERRUPT_CONFIG_GPIO, temp);
  status |= WrWord(dev, SYSTEM_THRESH_HIGH, threshold_high);
  status |= WrWord(dev, SYSTEM_THRESH_LOW, threshold_low);
  return status;
}

VL53L1X_ERROR SetROICenter(i2c::I2CDevice *dev, uint8_t roi_center) {
  VL53L1X_ERROR status = 0;
  status |= WrByte(dev, ROI_CONFIG_USER_ROI_CENTRE_SPAD, roi_center);
  return status;
}

VL53L1X_ERROR SetROI(i2c::I2CDevice *dev, uint16_t x, uint16_t y) {
  uint8_t optical_center = 0;
  VL53L1X_ERROR status = 0;

  status |= RdByte(dev, ROI_CONFIG_MODE_ROI_CENTRE_SPAD, &optical_center);
  if (x > 16)
    x = 16;
  if (y > 16)
    y = 16;
  if (x > 10 || y > 10) {
    optical_center = 199;
  }
  status |= WrByte(dev, ROI_CONFIG_USER_ROI_CENTRE_SPAD, optical_center);
  status |= WrByte(dev, ROI_CONFIG_USER_ROI_REQUESTED_GLOBAL_XY_SIZE, (y - 1) << 4 | (x - 1));
  return status;
}

VL53L1X_ERROR SetSignalThreshold(i2c::I2CDevice *dev, uint16_t signal_threshold) {
  VL53L1X_ERROR status = 0;

  status |= WrWord(dev, RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT_MCPS, signal_threshold >> 3);
  return status;
}

VL53L1X_ERROR SetSigmaThreshold(i2c::I2CDevice *dev, uint16_t sigma_threshold) {
  VL53L1X_ERROR status = 0;

  if (sigma_threshold > (0xFFFF >> 2)) {
    return 1;
  }
  /* 16 bits register 14.2 format */
  status |= WrWord(dev, RANGE_CONFIG_SIGMA_THRESH, sigma_threshold << 2);
  return status;
}

VL53L1X_ERROR StartTemperatureUpdate(i2c::I2CDevice *dev) {
  VL53L1X_ERROR status = 0;
  uint8_t tmp = 0;
  uint16_t timeout_counter = 0;

  status |= WrByte(dev, VHV_CONFIG_TIMEOUT_MACROP_LOOP_BOUND, 0x81); /* full VHV */
  status |= WrByte(dev, 0x0B, 0x92);
  status |= StartRanging(dev);
  while (tmp == 0) {
    status = CheckForDataReady(dev, &tmp);
    timeout_counter++;
    if (timeout_counter >= 1000) {
      status = (uint8_t) VL53L1X_ERROR_TIMEOUT;
      return status;
    }
    status = WaitMs(dev, 1);
  }
  status |= ClearInterrupt(dev);
  status |= StopRanging(dev);
  status |= WrByte(dev, VHV_CONFIG_TIMEOUT_MACROP_LOOP_BOUND, 0x09); /* two bounds VHV */
  status |= WrByte(dev, 0x0B, 0);                                    /* start VHV from the previous temperature */
  return status;
}

}  // namespace driver
}  // namespace vl53l1x
}  // namespace esphome
