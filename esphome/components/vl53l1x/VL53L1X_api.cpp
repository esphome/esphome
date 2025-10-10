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
 * @file  vl53l1x_api.c
 * @brief Functions implementation
 */

#include "VL53L1X_api.h"
#include <string.h>

namespace esphome {
namespace vl53l1x {
namespace st_vl53l1x_uld {
#if 0
uint8_t VL51L1X_NVM_CONFIGURATION[] = {
0x00, /* 0x00 : not user-modifiable */
0x29, /* 0x01 : 7 bits I2C address (default=0x29), use SetI2CAddress(). Warning: after changing the register value to a new I2C address, the device will only answer to the new address */
0x00, /* 0x02 : not user-modifiable */
0x00, /* 0x03 : not user-modifiable */
0x00, /* 0x04 : not user-modifiable */
0x00, /* 0x05 : not user-modifiable */
0x00, /* 0x06 : not user-modifiable */
0x00, /* 0x07 : not user-modifiable */
0x00, /* 0x08 : not user-modifiable */
0x50, /* 0x09 : not user-modifiable */
0x00, /* 0x0A : not user-modifiable */
0x00, /* 0x0B : not user-modifiable */
0x00, /* 0x0C : not user-modifiable */
0x00, /* 0x0D : not user-modifiable */
0x0a, /* 0x0E : not user-modifiable */
0x00, /* 0x0F : not user-modifiable */
0x00, /* 0x10 : not user-modifiable */
0x00, /* 0x11 : not user-modifiable */
0x00, /* 0x12 : not user-modifiable */
0x00, /* 0x13 : not user-modifiable */
0x00, /* 0x14 : not user-modifiable */
0x00, /* 0x15 : not user-modifiable */
0x00, /* 0x16 : Xtalk calibration value MSB (7.9 format in kcps), use SetXtalk() */
0x00, /* 0x17 : Xtalk calibration value LSB */
0x00, /* 0x18 : not user-modifiable */
0x00, /* 0x19 : not user-modifiable */
0x00, /* 0x1a : not user-modifiable */
0x00, /* 0x1b : not user-modifiable */
0x00, /* 0x1e : Part to Part offset x4 MSB (in mm), use SetOffset() */
0x50, /* 0x1f : Part to Part offset x4 LSB */
0x00, /* 0x20 : not user-modifiable */
0x00, /* 0x21 : not user-modifiable */
0x00, /* 0x22 : not user-modifiable */
0x00, /* 0x23 : not user-modifiable */
}
#endif

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
             VL53L1X_init() call, put 0x40 in location 0x87 */
};

static const uint8_t status_rtn[24] = {255, 255, 255, 5,   2,   4,   1,  7, 3,   0,   255, 255,
                                       9,   13,  255, 255, 255, 255, 10, 6, 255, 255, 11,  12};

#ifdef VL53L1X_INCLUDE_READ_FUNCTIONS
VL53L1X_ERROR VL53L1X_GetSWVersion(VL53L1X_Version_t *pVersion) {
  VL53L1X_ERROR Status = 0;

  pVersion->major = VL53L1X_IMPLEMENTATION_VER_MAJOR;
  pVersion->minor = VL53L1X_IMPLEMENTATION_VER_MINOR;
  pVersion->build = VL53L1X_IMPLEMENTATION_VER_SUB;
  pVersion->revision = VL53L1X_IMPLEMENTATION_VER_REVISION;
  return Status;
}
#endif

VL53L1X_ERROR VL53L1X_SetI2CAddress(uint16_t dev, uint8_t new_address) {
  VL53L1X_ERROR status = 0;

  status |= VL53L1_WrByte(dev, VL53L1_I2C__DEVICE_ADDRESS, new_address >> 1);
  return status;
}

VL53L1X_ERROR VL53L1X_SensorInit(uint16_t dev) {
  VL53L1X_ERROR status = 0;
  uint8_t Addr = 0x00, tmp = 0;
  uint16_t timeout_counter = 0;

  for (Addr = 0x2D; Addr <= 0x87; Addr++) {
    status |= VL53L1_WrByte(dev, Addr, VL51L1X_DEFAULT_CONFIGURATION[Addr - 0x2D]);
  }
  status |= VL53L1X_StartRanging(dev);
  while (tmp == 0) {
    status = VL53L1X_CheckForDataReady(dev, &tmp);
    timeout_counter++;
    if (timeout_counter >= 1000) {
      status = (uint8_t) VL53L1X_ERROR_TIMEOUT;
      return status;
    }
    status = VL53L1_WaitMs(dev, 1);
  }
  status |= VL53L1X_ClearInterrupt(dev);
  status |= VL53L1X_StopRanging(dev);
  status |= VL53L1_WrByte(dev, VL53L1_VHV_CONFIG__TIMEOUT_MACROP_LOOP_BOUND, 0x09); /* two bounds VHV */
  status |= VL53L1_WrByte(dev, 0x0B, 0); /* start VHV from the previous temperature */
  return status;
}

VL53L1X_ERROR VL53L1X_ClearInterrupt(uint16_t dev) {
  VL53L1X_ERROR status = 0;

  status |= VL53L1_WrByte(dev, SYSTEM__INTERRUPT_CLEAR, 0x01);
  return status;
}

VL53L1X_ERROR VL53L1X_SetInterruptPolarity(uint16_t dev, uint8_t NewPolarity) {
  uint8_t Temp;
  VL53L1X_ERROR status = 0;

  status |= VL53L1_RdByte(dev, GPIO_HV_MUX__CTRL, &Temp);
  Temp = Temp & 0xEF;
  status |= VL53L1_WrByte(dev, GPIO_HV_MUX__CTRL, Temp | (!(NewPolarity & 1)) << 4);
  return status;
}

VL53L1X_ERROR VL53L1X_GetInterruptPolarity(uint16_t dev, uint8_t *pInterruptPolarity) {
  uint8_t Temp;
  VL53L1X_ERROR status = 0;

  status |= VL53L1_RdByte(dev, GPIO_HV_MUX__CTRL, &Temp);
  Temp = Temp & 0x10;
  *pInterruptPolarity = !(Temp >> 4);
  return status;
}

VL53L1X_ERROR VL53L1X_StartRanging(uint16_t dev) {
  VL53L1X_ERROR status = 0;

  status |= VL53L1_WrByte(dev, SYSTEM__MODE_START, 0x40); /* Enable VL53L1X */
  return status;
}

VL53L1X_ERROR VL53L1X_StopRanging(uint16_t dev) {
  VL53L1X_ERROR status = 0;

  status |= VL53L1_WrByte(dev, SYSTEM__MODE_START, 0x00); /* Disable VL53L1X */
  return status;
}

VL53L1X_ERROR VL53L1X_CheckForDataReady(uint16_t dev, uint8_t *isDataReady) {
  uint8_t Temp;
  uint8_t IntPol;
  VL53L1X_ERROR status = 0;

  status |= VL53L1X_GetInterruptPolarity(dev, &IntPol);
  status |= VL53L1_RdByte(dev, GPIO__TIO_HV_STATUS, &Temp);
  /* Read in the register to check if a new value is available */
  if (status == 0) {
    if ((Temp & 1) == IntPol)
      *isDataReady = 1;
    else
      *isDataReady = 0;
  }
  return status;
}

VL53L1X_ERROR VL53L1X_SetTimingBudgetInMs(uint16_t dev, uint16_t TimingBudgetInMs) {
  uint16_t DM;
  VL53L1X_ERROR status = 0;

  status |= VL53L1X_GetDistanceMode(dev, &DM);
  if (DM == 0)
    return 1;
  else if (DM == 1) { /* Short DistanceMode */
    switch (TimingBudgetInMs) {
      case 15: /* only available in short distance mode */
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x01D);
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x0027);
        break;
      case 20:
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x0051);
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x006E);
        break;
      case 33:
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x00D6);
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x006E);
        break;
      case 50:
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x1AE);
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x01E8);
        break;
      case 100:
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x02E1);
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x0388);
        break;
      case 200:
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x03E1);
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x0496);
        break;
      case 500:
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x0591);
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x05C1);
        break;
      default:
        status = 1;
        break;
    }
  } else {
    switch (TimingBudgetInMs) {
      case 20:
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x001E);
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x0022);
        break;
      case 33:
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x0060);
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x006E);
        break;
      case 50:
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x00AD);
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x00C6);
        break;
      case 100:
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x01CC);
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x01EA);
        break;
      case 200:
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x02D9);
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x02F8);
        break;
      case 500:
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x048F);
        VL53L1_WrWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x04A4);
        break;
      default:
        status = 1;
        break;
    }
  }
  return status;
}

VL53L1X_ERROR VL53L1X_GetTimingBudgetInMs(uint16_t dev, uint16_t *pTimingBudget) {
  uint16_t Temp;
  VL53L1X_ERROR status = 0;

  status |= VL53L1_RdWord(dev, RANGE_CONFIG__TIMEOUT_MACROP_A_HI, &Temp);
  switch (Temp) {
    case 0x001D:
      *pTimingBudget = 15;
      break;
    case 0x0051:
    case 0x001E:
      *pTimingBudget = 20;
      break;
    case 0x00D6:
    case 0x0060:
      *pTimingBudget = 33;
      break;
    case 0x1AE:
    case 0x00AD:
      *pTimingBudget = 50;
      break;
    case 0x02E1:
    case 0x01CC:
      *pTimingBudget = 100;
      break;
    case 0x03E1:
    case 0x02D9:
      *pTimingBudget = 200;
      break;
    case 0x0591:
    case 0x048F:
      *pTimingBudget = 500;
      break;
    default:
      status = 1;
      *pTimingBudget = 0;
  }
  return status;
}

VL53L1X_ERROR VL53L1X_SetDistanceMode(uint16_t dev, uint16_t DM) {
  uint16_t TB;
  VL53L1X_ERROR status = 0;

  status |= VL53L1X_GetTimingBudgetInMs(dev, &TB);
  if (status != 0)
    return 1;
  switch (DM) {
    case 1:
      status = VL53L1_WrByte(dev, PHASECAL_CONFIG__TIMEOUT_MACROP, 0x14);
      status = VL53L1_WrByte(dev, RANGE_CONFIG__VCSEL_PERIOD_A, 0x07);
      status = VL53L1_WrByte(dev, RANGE_CONFIG__VCSEL_PERIOD_B, 0x05);
      status = VL53L1_WrByte(dev, RANGE_CONFIG__VALID_PHASE_HIGH, 0x38);
      status = VL53L1_WrWord(dev, SD_CONFIG__WOI_SD0, 0x0705);
      status = VL53L1_WrWord(dev, SD_CONFIG__INITIAL_PHASE_SD0, 0x0606);
      break;
    case 2:
      status = VL53L1_WrByte(dev, PHASECAL_CONFIG__TIMEOUT_MACROP, 0x0A);
      status = VL53L1_WrByte(dev, RANGE_CONFIG__VCSEL_PERIOD_A, 0x0F);
      status = VL53L1_WrByte(dev, RANGE_CONFIG__VCSEL_PERIOD_B, 0x0D);
      status = VL53L1_WrByte(dev, RANGE_CONFIG__VALID_PHASE_HIGH, 0xB8);
      status = VL53L1_WrWord(dev, SD_CONFIG__WOI_SD0, 0x0F0D);
      status = VL53L1_WrWord(dev, SD_CONFIG__INITIAL_PHASE_SD0, 0x0E0E);
      break;
    default:
      status = 1;
      break;
  }

  if (status == 0)
    status |= VL53L1X_SetTimingBudgetInMs(dev, TB);
  return status;
}

VL53L1X_ERROR VL53L1X_GetDistanceMode(uint16_t dev, uint16_t *DM) {
  uint8_t TempDM, status = 0;

  status |= VL53L1_RdByte(dev, PHASECAL_CONFIG__TIMEOUT_MACROP, &TempDM);
  if (TempDM == 0x14)
    *DM = 1;
  if (TempDM == 0x0A)
    *DM = 2;
  return status;
}

VL53L1X_ERROR VL53L1X_SetInterMeasurementInMs(uint16_t dev, uint32_t InterMeasMs) {
  uint16_t ClockPLL;
  VL53L1X_ERROR status = 0;

  status |= VL53L1_RdWord(dev, VL53L1_RESULT__OSC_CALIBRATE_VAL, &ClockPLL);
  ClockPLL = ClockPLL & 0x3FF;
  VL53L1_WrDWord(dev, VL53L1_SYSTEM__INTERMEASUREMENT_PERIOD, (uint32_t) (ClockPLL * InterMeasMs * 1.075));
  return status;
}

#ifdef VL53L1X_INCLUDE_READ_FUNCTIONS
VL53L1X_ERROR VL53L1X_GetInterMeasurementInMs(uint16_t dev, uint16_t *pIM) {
  uint16_t ClockPLL;
  VL53L1X_ERROR status = 0;
  uint32_t tmp;

  status |= VL53L1_RdDWord(dev, VL53L1_SYSTEM__INTERMEASUREMENT_PERIOD, &tmp);
  *pIM = (uint16_t) tmp;
  status |= VL53L1_RdWord(dev, VL53L1_RESULT__OSC_CALIBRATE_VAL, &ClockPLL);
  ClockPLL = ClockPLL & 0x3FF;
  *pIM = (uint16_t) (*pIM / (ClockPLL * 1.065));
  return status;
}
#endif

VL53L1X_ERROR VL53L1X_BootState(uint16_t dev, uint8_t *state) {
  VL53L1X_ERROR status = 0;
  uint8_t tmp = 0;

  status |= VL53L1_RdByte(dev, VL53L1_FIRMWARE__SYSTEM_STATUS, &tmp);
  *state = tmp;
  return status;
}

#ifdef VL53L1X_INCLUDE_READ_FUNCTIONS
VL53L1X_ERROR VL53L1X_GetSensorId(uint16_t dev, uint16_t *sensorId) {
  VL53L1X_ERROR status = 0;
  uint16_t tmp = 0;

  status |= VL53L1_RdWord(dev, VL53L1_IDENTIFICATION__MODEL_ID, &tmp);
  *sensorId = tmp;
  return status;
}
#endif

VL53L1X_ERROR VL53L1X_GetDistance(uint16_t dev, uint16_t *distance) {
  VL53L1X_ERROR status = 0;
  uint16_t tmp;

  status |= (VL53L1_RdWord(dev, VL53L1_RESULT__FINAL_CROSSTALK_CORRECTED_RANGE_MM_SD0, &tmp));
  *distance = tmp;
  return status;
}

#ifdef VL53L1X_INCLUDE_READ_FUNCTIONS
VL53L1X_ERROR VL53L1X_GetSignalPerSpad(uint16_t dev, uint16_t *signalRate) {
  VL53L1X_ERROR status = 0;
  uint16_t SpNb = 1, signal;

  status |= VL53L1_RdWord(dev, VL53L1_RESULT__PEAK_SIGNAL_COUNT_RATE_CROSSTALK_CORRECTED_MCPS_SD0, &signal);
  status |= VL53L1_RdWord(dev, VL53L1_RESULT__DSS_ACTUAL_EFFECTIVE_SPADS_SD0, &SpNb);
  *signalRate = (uint16_t) (200.0 * signal / SpNb);
  return status;
}

VL53L1X_ERROR VL53L1X_GetAmbientPerSpad(uint16_t dev, uint16_t *ambPerSp) {
  VL53L1X_ERROR status = 0;
  uint16_t AmbientRate, SpNb = 1;

  status |= VL53L1_RdWord(dev, RESULT__AMBIENT_COUNT_RATE_MCPS_SD, &AmbientRate);
  status |= VL53L1_RdWord(dev, VL53L1_RESULT__DSS_ACTUAL_EFFECTIVE_SPADS_SD0, &SpNb);
  *ambPerSp = (uint16_t) (200.0 * AmbientRate / SpNb);
  return status;
}

VL53L1X_ERROR VL53L1X_GetSignalRate(uint16_t dev, uint16_t *signal) {
  VL53L1X_ERROR status = 0;
  uint16_t tmp;

  status |= VL53L1_RdWord(dev, VL53L1_RESULT__PEAK_SIGNAL_COUNT_RATE_CROSSTALK_CORRECTED_MCPS_SD0, &tmp);
  *signal = tmp * 8;
  return status;
}

VL53L1X_ERROR VL53L1X_GetSpadNb(uint16_t dev, uint16_t *spNb) {
  VL53L1X_ERROR status = 0;
  uint16_t tmp;

  status |= VL53L1_RdWord(dev, VL53L1_RESULT__DSS_ACTUAL_EFFECTIVE_SPADS_SD0, &tmp);
  *spNb = tmp >> 8;
  return status;
}

VL53L1X_ERROR VL53L1X_GetAmbientRate(uint16_t dev, uint16_t *ambRate) {
  VL53L1X_ERROR status = 0;
  uint16_t tmp;

  status |= VL53L1_RdWord(dev, RESULT__AMBIENT_COUNT_RATE_MCPS_SD, &tmp);
  *ambRate = tmp * 8;
  return status;
}
#endif

VL53L1X_ERROR VL53L1X_GetRangeStatus(uint16_t dev, uint8_t *rangeStatus) {
  VL53L1X_ERROR status = 0;
  uint8_t RgSt;

  *rangeStatus = 255;
  status |= VL53L1_RdByte(dev, VL53L1_RESULT__RANGE_STATUS, &RgSt);
  RgSt = RgSt & 0x1F;
  if (RgSt < 24)
    *rangeStatus = status_rtn[RgSt];
  return status;
}

#ifdef VL53L1X_INCLUDE_READ_FUNCTIONS
VL53L1X_ERROR VL53L1X_GetResult(uint16_t dev, VL53L1X_Result_t *pResult) {
  VL53L1X_ERROR status = 0;
  uint8_t Temp[17];
  uint8_t RgSt = 255;

  status |= VL53L1_ReadMulti(dev, VL53L1_RESULT__RANGE_STATUS, Temp, 17);
  RgSt = Temp[0] & 0x1F;
  if (RgSt < 24)
    RgSt = status_rtn[RgSt];
  pResult->Status = RgSt;
  pResult->Ambient = (Temp[7] << 8 | Temp[8]) * 8;
  pResult->NumSPADs = Temp[3];
  pResult->SigPerSPAD = (Temp[15] << 8 | Temp[16]) * 8;
  pResult->Distance = Temp[13] << 8 | Temp[14];

  return status;
}
#endif

VL53L1X_ERROR VL53L1X_SetOffset(uint16_t dev, int16_t OffsetValue) {
  VL53L1X_ERROR status = 0;
  int16_t Temp;

  Temp = (OffsetValue * 4);
  status |= VL53L1_WrWord(dev, ALGO__PART_TO_PART_RANGE_OFFSET_MM, (uint16_t) Temp);
  status |= VL53L1_WrWord(dev, MM_CONFIG__INNER_OFFSET_MM, 0x0);
  status |= VL53L1_WrWord(dev, MM_CONFIG__OUTER_OFFSET_MM, 0x0);
  return status;
}

#ifdef VL53L1X_INCLUDE_READ_FUNCTIONS
VL53L1X_ERROR VL53L1X_GetOffset(uint16_t dev, int16_t *offset) {
  VL53L1X_ERROR status = 0;
  uint16_t Temp;

  status |= VL53L1_RdWord(dev, ALGO__PART_TO_PART_RANGE_OFFSET_MM, &Temp);
  Temp = Temp << 3;
  Temp = Temp >> 5;
  *offset = (int16_t) (Temp);

  if (*offset > 1024) {
    *offset = *offset - 2048;
  }

  return status;
}
#endif

VL53L1X_ERROR VL53L1X_SetXtalk(uint16_t dev, uint16_t XtalkValue) {
  /* XTalkValue in count per second to avoid float type */
  VL53L1X_ERROR status = 0;

  status |= VL53L1_WrWord(dev, ALGO__CROSSTALK_COMPENSATION_X_PLANE_GRADIENT_KCPS, 0x0000);
  status |= VL53L1_WrWord(dev, ALGO__CROSSTALK_COMPENSATION_Y_PLANE_GRADIENT_KCPS, 0x0000);
  status |= VL53L1_WrWord(dev, ALGO__CROSSTALK_COMPENSATION_PLANE_OFFSET_KCPS,
                          (XtalkValue << 9) / 1000); /* * << 9 (7.9 format) and /1000 to convert cps to kpcs */
  return status;
}

#ifdef VL53L1X_INCLUDE_READ_FUNCTIONS
VL53L1X_ERROR VL53L1X_GetXtalk(uint16_t dev, uint16_t *xtalk) {
  VL53L1X_ERROR status = 0;

  status |= VL53L1_RdWord(dev, ALGO__CROSSTALK_COMPENSATION_PLANE_OFFSET_KCPS, xtalk);
  *xtalk = (uint16_t) ((*xtalk * 1000) >> 9); /* * 1000 to convert kcps to cps and >> 9 (7.9 format) */
  return status;
}
#endif

VL53L1X_ERROR VL53L1X_SetDistanceThreshold(uint16_t dev, uint16_t ThreshLow, uint16_t ThreshHigh, uint8_t Window,
                                           uint8_t IntOnNoTarget) {
  VL53L1X_ERROR status = 0;
  uint8_t Temp = 0;

  status |= VL53L1_RdByte(dev, SYSTEM__INTERRUPT_CONFIG_GPIO, &Temp);
  Temp = Temp & (~0x6F);
  Temp = Temp | Window;
  if (IntOnNoTarget == 0) {
    status = VL53L1_WrByte(dev, SYSTEM__INTERRUPT_CONFIG_GPIO, Temp);
  } else {
    status = VL53L1_WrByte(dev, SYSTEM__INTERRUPT_CONFIG_GPIO, (Temp | 0x40));
  }
  status |= VL53L1_WrWord(dev, SYSTEM__THRESH_HIGH, ThreshHigh);
  status |= VL53L1_WrWord(dev, SYSTEM__THRESH_LOW, ThreshLow);
  return status;
}

#ifdef VL53L1X_INCLUDE_READ_FUNCTIONS
VL53L1X_ERROR VL53L1X_GetDistanceThresholdWindow(uint16_t dev, uint16_t *window) {
  VL53L1X_ERROR status = 0;
  uint8_t tmp;
  status |= VL53L1_RdByte(dev, SYSTEM__INTERRUPT_CONFIG_GPIO, &tmp);
  *window = (uint16_t) (tmp & 0x7);
  return status;
}

VL53L1X_ERROR VL53L1X_GetDistanceThresholdLow(uint16_t dev, uint16_t *low) {
  VL53L1X_ERROR status = 0;
  uint16_t tmp;

  status |= VL53L1_RdWord(dev, SYSTEM__THRESH_LOW, &tmp);
  *low = tmp;
  return status;
}

VL53L1X_ERROR VL53L1X_GetDistanceThresholdHigh(uint16_t dev, uint16_t *high) {
  VL53L1X_ERROR status = 0;
  uint16_t tmp;

  status |= VL53L1_RdWord(dev, SYSTEM__THRESH_HIGH, &tmp);
  *high = tmp;
  return status;
}
#endif

VL53L1X_ERROR VL53L1X_SetROICenter(uint16_t dev, uint8_t ROICenter) {
  VL53L1X_ERROR status = 0;
  status |= VL53L1_WrByte(dev, ROI_CONFIG__USER_ROI_CENTRE_SPAD, ROICenter);
  return status;
}

#ifdef VL53L1X_INCLUDE_READ_FUNCTIONS
VL53L1X_ERROR VL53L1X_GetROICenter(uint16_t dev, uint8_t *ROICenter) {
  VL53L1X_ERROR status = 0;
  uint8_t tmp;
  status |= VL53L1_RdByte(dev, ROI_CONFIG__USER_ROI_CENTRE_SPAD, &tmp);
  *ROICenter = tmp;
  return status;
}
#endif

VL53L1X_ERROR VL53L1X_SetROI(uint16_t dev, uint16_t X, uint16_t Y) {
  uint8_t OpticalCenter;
  VL53L1X_ERROR status = 0;

  status |= VL53L1_RdByte(dev, VL53L1_ROI_CONFIG__MODE_ROI_CENTRE_SPAD, &OpticalCenter);
  if (X > 16)
    X = 16;
  if (Y > 16)
    Y = 16;
  if (X > 10 || Y > 10) {
    OpticalCenter = 199;
  }
  status |= VL53L1_WrByte(dev, ROI_CONFIG__USER_ROI_CENTRE_SPAD, OpticalCenter);
  status |= VL53L1_WrByte(dev, ROI_CONFIG__USER_ROI_REQUESTED_GLOBAL_XY_SIZE, (Y - 1) << 4 | (X - 1));
  return status;
}

#ifdef VL53L1X_INCLUDE_READ_FUNCTIONS
VL53L1X_ERROR VL53L1X_GetROI_XY(uint16_t dev, uint16_t *ROI_X, uint16_t *ROI_Y) {
  VL53L1X_ERROR status = 0;
  uint8_t tmp;

  status = VL53L1_RdByte(dev, ROI_CONFIG__USER_ROI_REQUESTED_GLOBAL_XY_SIZE, &tmp);
  *ROI_X = ((uint16_t) tmp & 0x0F) + 1;
  *ROI_Y = (((uint16_t) tmp & 0xF0) >> 4) + 1;
  return status;
}
#endif

VL53L1X_ERROR VL53L1X_SetSignalThreshold(uint16_t dev, uint16_t Signal) {
  VL53L1X_ERROR status = 0;

  status |= VL53L1_WrWord(dev, RANGE_CONFIG__MIN_COUNT_RATE_RTN_LIMIT_MCPS, Signal >> 3);
  return status;
}

#ifdef VL53L1X_INCLUDE_READ_FUNCTIONS
VL53L1X_ERROR VL53L1X_GetSignalThreshold(uint16_t dev, uint16_t *signal) {
  VL53L1X_ERROR status = 0;
  uint16_t tmp;

  status |= VL53L1_RdWord(dev, RANGE_CONFIG__MIN_COUNT_RATE_RTN_LIMIT_MCPS, &tmp);
  *signal = tmp << 3;
  return status;
}
#endif

VL53L1X_ERROR VL53L1X_SetSigmaThreshold(uint16_t dev, uint16_t Sigma) {
  VL53L1X_ERROR status = 0;

  if (Sigma > (0xFFFF >> 2)) {
    return 1;
  }
  /* 16 bits register 14.2 format */
  status |= VL53L1_WrWord(dev, RANGE_CONFIG__SIGMA_THRESH, Sigma << 2);
  return status;
}

#ifdef VL53L1X_INCLUDE_READ_FUNCTIONS
VL53L1X_ERROR VL53L1X_GetSigmaThreshold(uint16_t dev, uint16_t *sigma) {
  VL53L1X_ERROR status = 0;
  uint16_t tmp;

  status |= VL53L1_RdWord(dev, RANGE_CONFIG__SIGMA_THRESH, &tmp);
  *sigma = tmp >> 2;
  return status;
}
#endif

VL53L1X_ERROR VL53L1X_StartTemperatureUpdate(uint16_t dev) {
  VL53L1X_ERROR status = 0;
  uint8_t tmp = 0;
  uint16_t timeout_counter = 0;

  status |= VL53L1_WrByte(dev, VL53L1_VHV_CONFIG__TIMEOUT_MACROP_LOOP_BOUND, 0x81); /* full VHV */
  status |= VL53L1_WrByte(dev, 0x0B, 0x92);
  status |= VL53L1X_StartRanging(dev);
  while (tmp == 0) {
    status = VL53L1X_CheckForDataReady(dev, &tmp);
    timeout_counter++;
    if (timeout_counter >= 1000) {
      status = (uint8_t) VL53L1X_ERROR_TIMEOUT;
      return status;
    }
    status = VL53L1_WaitMs(dev, 1);
  }
  status |= VL53L1X_ClearInterrupt(dev);
  status |= VL53L1X_StopRanging(dev);
  status |= VL53L1_WrByte(dev, VL53L1_VHV_CONFIG__TIMEOUT_MACROP_LOOP_BOUND, 0x09); /* two bounds VHV */
  status |= VL53L1_WrByte(dev, 0x0B, 0); /* start VHV from the previous temperature */
  return status;
}

}  // namespace st_vl53l1x_uld
}  // namespace vl53l1x
}  // namespace esphome
