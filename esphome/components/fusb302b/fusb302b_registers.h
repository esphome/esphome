#pragma once

#include <cstdint>

namespace esphome::fusb302b {

// Register addresses
static constexpr uint8_t FUSB_DEVICE_ID = 0x01;
static constexpr uint8_t FUSB_SWITCHES0 = 0x02;
static constexpr uint8_t FUSB_SWITCHES1 = 0x03;
static constexpr uint8_t FUSB_MEASURE = 0x04;
static constexpr uint8_t FUSB_CONTROL0 = 0x06;
static constexpr uint8_t FUSB_CONTROL1 = 0x07;
static constexpr uint8_t FUSB_CONTROL3 = 0x09;
static constexpr uint8_t FUSB_MASK1 = 0x0A;
static constexpr uint8_t FUSB_POWER = 0x0B;
static constexpr uint8_t FUSB_RESET = 0x0C;
static constexpr uint8_t FUSB_MASKA = 0x0E;
static constexpr uint8_t FUSB_MASKB = 0x0F;
static constexpr uint8_t FUSB_STATUS0A = 0x3C;
static constexpr uint8_t FUSB_STATUS0 = 0x40;
static constexpr uint8_t FUSB_STATUS1 = 0x41;
static constexpr uint8_t FUSB_FIFOS = 0x43;

// DEVICE_ID values of the FUSB302B revisions
static constexpr uint8_t FUSB_DEVICE_ID_A = 0x81;
static constexpr uint8_t FUSB_DEVICE_ID_B = 0x91;

// SWITCHES0 bits
static constexpr uint8_t FUSB_SWITCHES0_MEAS_CC2 = 1 << 3;
static constexpr uint8_t FUSB_SWITCHES0_MEAS_CC1 = 1 << 2;
static constexpr uint8_t FUSB_SWITCHES0_PDWN_2 = 1 << 1;
static constexpr uint8_t FUSB_SWITCHES0_PDWN_1 = 1 << 0;

// SWITCHES1 bits
static constexpr uint8_t FUSB_SWITCHES1_SPECREV_REV2 = 0x01 << 5;
static constexpr uint8_t FUSB_SWITCHES1_AUTO_CRC = 1 << 2;
static constexpr uint8_t FUSB_SWITCHES1_TXCC2 = 1 << 1;
static constexpr uint8_t FUSB_SWITCHES1_TXCC1 = 1 << 0;

// MEASURE value used for CC detection
static constexpr uint8_t FUSB_MEASURE_MDAC_CC = 49;

// CONTROL0 bits
static constexpr uint8_t FUSB_CONTROL0_TX_FLUSH = 1 << 6;
static constexpr uint8_t FUSB_CONTROL0_INT_MASK = 1 << 5;

// CONTROL1 bits
static constexpr uint8_t FUSB_CONTROL1_RX_FLUSH = 1 << 2;

// CONTROL3 bits
static constexpr uint8_t FUSB_CONTROL3_N_RETRIES_SHIFT = 1;
static constexpr uint8_t FUSB_CONTROL3_N_RETRIES_MASK = 0x3 << FUSB_CONTROL3_N_RETRIES_SHIFT;
static constexpr uint8_t FUSB_CONTROL3_AUTO_RETRY = 1 << 0;

// MASK1: mask the ACTIVITY, CRC_CHK and BC_LVL interrupts
static constexpr uint8_t FUSB_MASK1_DEFAULT = 0x51;

// POWER bits
static constexpr uint8_t FUSB_POWER_ALL = 0x0F;

// RESET bits
static constexpr uint8_t FUSB_RESET_PD_RESET = 1 << 1;
static constexpr uint8_t FUSB_RESET_SW_RES = 1 << 0;

// INTERRUPTA bits
static constexpr uint8_t FUSB_INTERRUPTA_I_RETRYFAIL = 1 << 4;
static constexpr uint8_t FUSB_INTERRUPTA_I_SOFTRST = 1 << 1;
static constexpr uint8_t FUSB_INTERRUPTA_I_HARDRST = 1 << 0;

// INTERRUPT bits
static constexpr uint8_t FUSB_INTERRUPT_I_VBUSOK = 1 << 7;

// STATUS0 bits
static constexpr uint8_t FUSB_STATUS0_VBUSOK = 1 << 7;
static constexpr uint8_t FUSB_STATUS0_BC_LVL = 0x03;

// STATUS1 bits
static constexpr uint8_t FUSB_STATUS1_RX_EMPTY = 1 << 5;

// RX FIFO token
static constexpr uint8_t FUSB_FIFO_RX_TOKEN_BITS = 0xE0;
static constexpr uint8_t FUSB_FIFO_RX_SOP = 0xE0;

// TX FIFO tokens
static constexpr uint8_t FUSB_TX_TOKEN_TXON = 0xA1;
static constexpr uint8_t FUSB_TX_TOKEN_SYNC1 = 0x12;
static constexpr uint8_t FUSB_TX_TOKEN_SYNC2 = 0x13;
static constexpr uint8_t FUSB_TX_TOKEN_PACKSYM = 0x80;
static constexpr uint8_t FUSB_TX_TOKEN_JAM_CRC = 0xFF;
static constexpr uint8_t FUSB_TX_TOKEN_EOP = 0x14;
static constexpr uint8_t FUSB_TX_TOKEN_TXOFF = 0xFE;

}  // namespace esphome::fusb302b
