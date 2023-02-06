#pragma once
#include <stdint.h>

namespace esphome {
namespace LSM6DS3 {

static const uint8_t LSM6DS3_ACC_GYRO_CTRL1_XL =             0X10;
static const uint8_t LSM6DS3_ACC_GYRO_CTRL4_C =              0X13;
static const uint8_t LSM6DS3_ACC_GYRO_BW_SCAL_ODR_ENABLED =  0x80;
static const uint8_t LSM6DS3_PERF_CTRL6_C =                  0x15;
static const uint8_t LSM6DS3_HIGH_PEF_DISABLE =              0x10;
 
static const uint8_t LSM6DS3_ACC_GYRO_BW_XL_100Hz =  0x02;
static const uint8_t LSM6DS3_ACC_GYRO_FS_XL_16g =    0x04;
static const uint8_t LSM6DS3_ACC_GYRO_FS_G_2000dps = 0x0C;
static const uint8_t LSM6DS3_ACC_GYRO_CTRL2_G =      0X11;
static const uint8_t LSM6DS3_ACC_GYRO_WHO_AM_I_REG = 0X0F;
static const uint8_t LSM6DS3_ACC_GYRO_WHO_AM_I =     0X69;
static const uint8_t LSM6DS3_C_ACC_GYRO_WHO_AM_I =   0x6A;
static const uint8_t LSM6DS3_ACC_GYRO_OUTX_L_XL =    0X28;
static const uint8_t LSM6DS3_ACC_GYRO_OUTY_L_XL =    0X2A;
static const uint8_t LSM6DS3_ACC_GYRO_OUTZ_L_XL =    0X2C;
static const uint8_t LSM6DS3_ACC_GYRO_OUTX_L_G =     0X22;
static const uint8_t LSM6DS3_ACC_GYRO_OUTY_L_G =     0X24;
static const uint8_t LSM6DS3_ACC_GYRO_OUTZ_L_G =     0X26;
static const uint8_t LSM6DS3_ACC_GYRO_OUT_TEMP_L =   0X20;


static const uint8_t LSM6DS3_ACC_GYRO_ODR_XL_POWER_DOWN =    0x00;
static const uint8_t LSM6DS3_ACC_GYRO_ODR_XL_13Hz =          0x10;
static const uint8_t LSM6DS3_ACC_GYRO_ODR_XL_26Hz =          0x20;
static const uint8_t LSM6DS3_ACC_GYRO_ODR_XL_52Hz =          0x30;
static const uint8_t LSM6DS3_ACC_GYRO_ODR_XL_104Hz =         0x40;
static const uint8_t LSM6DS3_ACC_GYRO_ODR_XL_208Hz =         0x50;
static const uint8_t LSM6DS3_ACC_GYRO_ODR_XL_416Hz =         0x60;
static const uint8_t LSM6DS3_ACC_GYRO_ODR_XL_833Hz =         0x70;
static const uint8_t LSM6DS3_ACC_GYRO_ODR_XL_1660Hz =        0x80;
static const uint8_t LSM6DS3_ACC_GYRO_ODR_XL_3330Hz =        0x90;
static const uint8_t LSM6DS3_ACC_GYRO_ODR_XL_6660Hz =        0xA0;
static const uint8_t LSM6DS3_ACC_GYRO_ODR_XL_13330Hz =       0xB0;

static const uint8_t LSM6DS3_ACC_GYRO_ODR_G_POWER_DOWN =     0x00;
static const uint8_t LSM6DS3_ACC_GYRO_ODR_G_13Hz =           0x10;
static const uint8_t LSM6DS3_ACC_GYRO_ODR_G_26Hz =           0x20;
static const uint8_t LSM6DS3_ACC_GYRO_ODR_G_52Hz =           0x30;
static const uint8_t LSM6DS3_ACC_GYRO_ODR_G_104Hz =          0x40;
static const uint8_t LSM6DS3_ACC_GYRO_ODR_G_208Hz =          0x50;
static const uint8_t LSM6DS3_ACC_GYRO_ODR_G_416Hz =          0x60;
static const uint8_t LSM6DS3_ACC_GYRO_ODR_G_833Hz =          0x70;
static const uint8_t LSM6DS3_ACC_GYRO_ODR_G_1660Hz =         0x80;

}  // namespace LSM6DS3
}  // namespace esphome