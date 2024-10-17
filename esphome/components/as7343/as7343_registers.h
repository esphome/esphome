#pragma once
#include <cstdint>

namespace esphome {
namespace as7343 {

enum class AS7343Registers : uint8_t {
  AUXID = 0x58,
  REVID = 0x59,
  ID = 0x5A,
  CFG12 = 0x66,
  ENABLE = 0x80,
  ATIME = 0x81,
  WTIME = 0x83,
  SP_TH_L_LSB = 0x84,
  SP_TH_L_MSB = 0x85,
  SP_TH_H_LSB = 0x86,
  SP_TH_H_MSB = 0x87,
  STATUS = 0x93,
  ASTATUS = 0x94,
  DATA_O = 0x95,
  STATUS2 = 0x90,
  STATUS3 = 0x91,
  STATUS5 = 0xBB,
  STATUS4 = 0xBC,
  CFG0 = 0xBF,
  CFG1 = 0xC6,
  CFG3 = 0xC7,
  CFG6 = 0xF5,
  CFG8 = 0xC9,
  CFG9 = 0xCA,
  CFG10 = 0x65,
  PERS = 0xCF,
  GPIO = 0x6B,
  ASTEP_LSB = 0xD4,
  ASTEP_MSB = 0xD5,
  CFG20 = 0xD6,
  LED = 0xCD,
  AGC_GAIN_MAX = 0xD7,
  AZ_CONFIG = 0xDE,
  FD_TIME_1 = 0xE0,
  FD_TIME_2 = 0xE2,
  FD_CFG0 = 0xDF,
  FD_STATUS = 0xE3,
  INTENAB = 0xF9,
  CONTROL = 0xFA,
  FIFO_MAP = 0xFC,
  FIFO_LVL = 0xFD,
  FDATA_L = 0xFE,
  FDATA_H = 0xFF,
};

static constexpr uint8_t AS7343_CHIP_ID = 0b10000001;
static constexpr uint8_t AS7343_ENABLE_PON_BIT = 0;
static constexpr uint8_t AS7343_ENABLE_SP_EN_BIT = 1;
static constexpr uint8_t AS7343_CFG0_REG_BANK_BIT = 4;

enum AS7343Gain : uint8_t {
  AS7343_GAIN_0_5X,
  AS7343_GAIN_1X,
  AS7343_GAIN_2X,
  AS7343_GAIN_4X,
  AS7343_GAIN_8X,
  AS7343_GAIN_16X,
  AS7343_GAIN_32X,
  AS7343_GAIN_64X,
  AS7343_GAIN_128X,
  AS7343_GAIN_256X,
  AS7343_GAIN_512X,
  AS7343_GAIN_1024X,
  AS7343_GAIN_2048X,
};

enum AS7343Channel : uint8_t {
  AS7343_CHANNEL_450_FZ,
  AS7343_CHANNEL_555_FY,
  AS7343_CHANNEL_600_FXL,
  AS7343_CHANNEL_855_NIR,
  AS7343_CHANNEL_CLEAR_1,
  AS7343_CHANNEL_FD_1,
  AS7343_CHANNEL_425_F2,
  AS7343_CHANNEL_475_F3,
  AS7343_CHANNEL_515_F4,
  AS7343_CHANNEL_640_F6,
  AS7343_CHANNEL_CLEAR_0,
  AS7343_CHANNEL_FD_0,
  AS7343_CHANNEL_405_F1,
  AS7343_CHANNEL_550_F5,
  AS7343_CHANNEL_690_F7,
  AS7343_CHANNEL_745_F8,
  AS7343_CHANNEL_CLEAR,
  AS7343_CHANNEL_FD,
};

static constexpr uint8_t AS7343_NUM_CHANNELS_MAX = 18;
static constexpr uint8_t AS7343_NUM_CHANNELS = 13;

union AS7343RegCfg20 {
  uint8_t raw;
  struct {
    uint8_t reserved : 5;
    uint8_t auto_smux : 2;
    uint8_t fd_fifo_8b : 1;
  } __attribute__((packed));
};

union AS7343RegStatus {
  uint8_t raw;
  struct {
    uint8_t sint : 1;
    uint8_t reserved_1 : 1;
    uint8_t fint : 1;
    uint8_t aint : 1;
    uint8_t reserved_4_6 : 3;
    uint8_t asat : 1;
  } __attribute__((packed));
};

union AS7343RegStatus2 {
  uint8_t raw;
  struct {
    uint8_t fdsat_digital : 1;
    uint8_t fdsat_analog : 1;
    uint8_t reserved_2 : 1;
    uint8_t asat_analog : 1;
    uint8_t asat_digital : 1;
    uint8_t reserved_5 : 1;
    uint8_t avalid : 1;
    uint8_t reserved_7 : 1;
  } __attribute__((packed));
};

union AS7343RegAStatus {
  uint8_t raw;
  struct {
    AS7343Gain again_status : 4;
    uint8_t reserved_4_6 : 3;
    uint8_t asat_status : 1;

  } __attribute__((packed));
};

}  // namespace as7343
}  // namespace esphome
