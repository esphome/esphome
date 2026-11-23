#pragma once

#include "esphome/core/hal.h"

namespace esphome::sx1231 {

enum SX1231Reg : uint8_t {
  REG_FIFO = 0x00,
  REG_OP_MODE = 0x01,
  REG_DATA_MODUL = 0x02,
  REG_BITRATE_MSB = 0x03,
  REG_BITRATE_LSB = 0x04,
  REG_FDEV_MSB = 0x05,
  REG_FDEV_LSB = 0x06,
  REG_FRF_MSB = 0x07,
  REG_FRF_MID = 0x08,
  REG_FRF_LSB = 0x09,
  REG_VERSION = 0x10,
  REG_PA_LEVEL = 0x11,
  REG_PA_RAMP = 0x12,
  REG_OCP = 0x13,
  REG_LNA = 0x18,
  REG_RX_BW = 0x19,
  REG_RSSI_VALUE = 0x24,
  REG_DIO_MAPPING1 = 0x25,
  REG_DIO_MAPPING2 = 0x26,
  REG_IRQ_FLAGS1 = 0x27,
  REG_IRQ_FLAGS2 = 0x28,
  REG_RSSI_THRESH = 0x29,
  REG_RX_TIMEOUT2 = 0x2B,
  REG_PREAMBLE_SIZE_MSB = 0x2C,
  REG_PREAMBLE_SIZE_LSB = 0x2D,
  REG_SYNC_CONFIG = 0x2E,
  REG_SYNC_VALUE1 = 0x2F,
  REG_PACKET_CONFIG1 = 0x37,
  REG_PAYLOAD_LENGTH = 0x38,
  REG_FIFO_THRESH = 0x3C,
  REG_PACKET_CONFIG2 = 0x3D,
  REG_TEST_PA1 = 0x5A,
  REG_TEST_PA2 = 0x5C,
  REG_TEST_DAGC = 0x6F,
};

enum SX1231OpMode : uint8_t {
  MODE_RX = 0x10,
  MODE_TX = 0x0C,
  MODE_STDBY = 0x04,
  MODE_SLEEP = 0x00,
};

enum SX1231DataModul : uint8_t {
  CONTINUOUS_BIT_SYNC_OFF = 0x60,
  CONTINUOUS_BIT_SYNC_ON = 0x40,
  PACKET_MODE = 0x00,
  MOD_OOK = 0x08,
  MOD_FSK = 0x00,
  GAUSSIAN_BT_0_3 = 0x03,
  GAUSSIAN_BT_0_5 = 0x02,
  GAUSSIAN_BT_1_0 = 0x01,
  CUTOFF_BR_X_2 = 0x02,
  CUTOFF_BR_X_1 = 0x01,
  SHAPING_NONE = 0x00,
};

enum SX1231PaLevel : uint8_t {
  PA_PIN_RFO = 0x80,
  PA_PIN_BOOST = 0x40,
  PA_BOOST_COMBINED = 0x20,
};

enum SX1231PaRamp : uint8_t {
  PA_RAMP_10 = 0x0F,
  PA_RAMP_12 = 0x0E,
  PA_RAMP_15 = 0x0D,
  PA_RAMP_20 = 0x0C,
  PA_RAMP_25 = 0x0B,
  PA_RAMP_31 = 0x0A,
  PA_RAMP_40 = 0x09,
  PA_RAMP_50 = 0x08,
  PA_RAMP_62 = 0x07,
  PA_RAMP_100 = 0x06,
  PA_RAMP_125 = 0x05,
  PA_RAMP_250 = 0x04,
  PA_RAMP_500 = 0x03,
  PA_RAMP_1000 = 0x02,
  PA_RAMP_2000 = 0x01,
  PA_RAMP_3400 = 0x00,
};

enum SX1231Ocp : uint8_t {
  OCP_ON_95MA = 0x1A,
  OCP_OFF = 0x0F,
};

enum SX1231Lna : uint8_t {
  LNA_ZIN_200 = 0x80,
};

enum SX1231RxBw : uint8_t {
  DCC_FREQ_4_PERCENT = 0x40,
  RX_BW_MANT_24 = 0x10,
  RX_BW_MANT_20 = 0x08,
  RX_BW_MANT_16 = 0x00,
};

enum SX1231DioMapping1 : uint8_t {
  // In packet mode: PacketSent while sending, CrcOk while receiving
  DIO0_MAPPING_00 = 0x00,
  // In packet mode: PayloadReady while receiving
  DIO0_MAPPING_01 = 0x40,
};

enum SX1231DioMapping2 : uint8_t {
  CLK_OUT_OFF = 0x07,
};

enum SX1231IrqFlags1 : uint8_t {
  MODE_READY = 0x80,
  TIMEOUT = 0x04,
  SYNC_ADDRESS_MATCH = 0x01,
};

enum SX1231IrqFlags2 : uint8_t {
  FIFO_OVERRUN = 0x10,
  PAYLOAD_READY = 0x04,
};

enum SX1231SyncConfig : uint8_t {
  SYNC_ON = 0x80,
  SYNC_SIZE_SHIFT = 3,
};

enum SX1231PacketConfig1 : uint8_t {
  VARIABLE_LENGTH = 0x80,
  FIXED_LENGTH = 0x00,
  CRC_ON = 0x10,
  CRC_OFF = 0x00,
};

enum SX1231FifoThresh : uint8_t {
  TX_START_FIFO_NOT_EMPTY = 0x80,
  FIFO_THRESHOLD_DEFAULT = 0x0F,
};

enum SX1231PacketConfig2 : uint8_t {
  RESTART_RX = 0x04,
  AUTO_RX_RESTART_ON = 0x02,
};

enum SX1231TestPa : uint8_t {
  TEST_PA1_NORMAL = 0x55,
  TEST_PA1_HIGH_POWER = 0x5D,
  TEST_PA2_NORMAL = 0x70,
  TEST_PA2_HIGH_POWER = 0x7C,
};

enum SX1231TestDagc : uint8_t {
  DAGC_IMPROVED_MARGIN = 0x30,
};

}  // namespace esphome::sx1231
