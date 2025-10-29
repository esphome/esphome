#pragma once

#include "es8156.h"

namespace esphome {
namespace es8156 {

/* ES8156 register addresses */
/*
 * RESET Control
 */
static const uint8_t ES8156_REG00_RESET = 0x00;
/*
 * Clock Managerment
 */
static const uint8_t ES8156_REG01_MAINCLOCK_CTL = 0x01;
static const uint8_t ES8156_REG02_SCLK_MODE = 0x02;
static const uint8_t ES8156_REG03_LRCLK_DIV_H = 0x03;
static const uint8_t ES8156_REG04_LRCLK_DIV_L = 0x04;
static const uint8_t ES8156_REG05_SCLK_DIV = 0x05;
static const uint8_t ES8156_REG06_NFS_CONFIG = 0x06;
static const uint8_t ES8156_REG07_MISC_CONTROL1 = 0x07;
static const uint8_t ES8156_REG08_CLOCK_ON_OFF = 0x08;
static const uint8_t ES8156_REG09_MISC_CONTROL2 = 0x09;
static const uint8_t ES8156_REG0A_TIME_CONTROL1 = 0x0a;
static const uint8_t ES8156_REG0B_TIME_CONTROL2 = 0x0b;
/*
 * System Control
 */
static const uint8_t ES8156_REG0C_CHIP_STATUS = 0x0c;
static const uint8_t ES8156_REG0D_P2S_CONTROL = 0x0d;
static const uint8_t ES8156_REG10_DAC_OSR_COUNTER = 0x10;
/*
 * SDP Control
 */
static const uint8_t ES8156_REG11_DAC_SDP = 0x11;
static const uint8_t ES8156_REG12_AUTOMUTE_SET = 0x12;
static const uint8_t ES8156_REG13_DAC_MUTE = 0x13;
static const uint8_t ES8156_REG14_VOLUME_CONTROL = 0x14;

/*
 * ALC Control
 */
static const uint8_t ES8156_REG15_ALC_CONFIG1 = 0x15;
static const uint8_t ES8156_REG16_ALC_CONFIG2 = 0x16;
static const uint8_t ES8156_REG17_ALC_CONFIG3 = 0x17;
static const uint8_t ES8156_REG18_MISC_CONTROL3 = 0x18;
static const uint8_t ES8156_REG19_EQ_CONTROL1 = 0x19;
static const uint8_t ES8156_REG1A_EQ_CONTROL2 = 0x1a;
/*
 * Analog System Control
 */
static const uint8_t ES8156_REG20_ANALOG_SYS1 = 0x20;
static const uint8_t ES8156_REG21_ANALOG_SYS2 = 0x21;
static const uint8_t ES8156_REG22_ANALOG_SYS3 = 0x22;
static const uint8_t ES8156_REG23_ANALOG_SYS4 = 0x23;
static const uint8_t ES8156_REG24_ANALOG_LP = 0x24;
static const uint8_t ES8156_REG25_ANALOG_SYS5 = 0x25;
/*
 * Chip Information
 */
static const uint8_t ES8156_REGFC_I2C_PAGESEL = 0xFC;
static const uint8_t ES8156_REGFD_CHIPID1 = 0xFD;
static const uint8_t ES8156_REGFE_CHIPID0 = 0xFE;
static const uint8_t ES8156_REGFF_CHIP_VERSION = 0xFF;

}  // namespace es8156
}  // namespace esphome
