#pragma once
#include <cstdint>

namespace esphome {
namespace fusb302b {

// Register addresses
static constexpr uint8_t FUSB_DEVICE_ID = 0x01;
static constexpr uint8_t FUSB_SWITCHES0 = 0x02;
static constexpr uint8_t FUSB_SWITCHES1 = 0x03;
static constexpr uint8_t FUSB_MEASURE = 0x04;
static constexpr uint8_t FUSB_SLICE = 0x05;
static constexpr uint8_t FUSB_CONTROL0 = 0x06;
static constexpr uint8_t FUSB_CONTROL1 = 0x07;
static constexpr uint8_t FUSB_CONTROL2 = 0x08;
static constexpr uint8_t FUSB_CONTROL3 = 0x09;
static constexpr uint8_t FUSB_MASK1 = 0x0A;
static constexpr uint8_t FUSB_POWER = 0x0B;
static constexpr uint8_t FUSB_RESET = 0x0C;
static constexpr uint8_t FUSB_OCPREG = 0x0D;
static constexpr uint8_t FUSB_MASKA = 0x0E;
static constexpr uint8_t FUSB_MASKB = 0x0F;
static constexpr uint8_t FUSB_CONTROL4 = 0x10;
static constexpr uint8_t FUSB_STATUS0A = 0x3C;
static constexpr uint8_t FUSB_STATUS1A = 0x3D;
static constexpr uint8_t FUSB_INTERRUPTA = 0x3E;
static constexpr uint8_t FUSB_INTERRUPTB = 0x3F;
static constexpr uint8_t FUSB_STATUS0 = 0x40;
static constexpr uint8_t FUSB_STATUS1 = 0x41;
static constexpr uint8_t FUSB_INTERRUPT = 0x42;
static constexpr uint8_t FUSB_FIFOS = 0x43;

// SWITCHES0 bits
static constexpr uint8_t FUSB_SWITCHES0_PU_EN2 = (1 << 7);
static constexpr uint8_t FUSB_SWITCHES0_PU_EN1 = (1 << 6);
static constexpr uint8_t FUSB_SWITCHES0_VCONN_CC2 = (1 << 5);
static constexpr uint8_t FUSB_SWITCHES0_VCONN_CC1 = (1 << 4);
static constexpr uint8_t FUSB_SWITCHES0_MEAS_CC2 = (1 << 3);
static constexpr uint8_t FUSB_SWITCHES0_MEAS_CC1 = (1 << 2);
static constexpr uint8_t FUSB_SWITCHES0_PDWN_2 = (1 << 1);
static constexpr uint8_t FUSB_SWITCHES0_PDWN_1 = 1;

// SWITCHES1 bits
static constexpr uint8_t FUSB_SWITCHES1_POWERROLE = (1 << 7);
static constexpr uint8_t FUSB_SWITCHES1_SPECREV_SHIFT = 5;
static constexpr uint8_t FUSB_SWITCHES1_SPECREV = (0x3 << FUSB_SWITCHES1_SPECREV_SHIFT);
static constexpr uint8_t FUSB_SWITCHES1_DATAROLE = (1 << 4);
static constexpr uint8_t FUSB_SWITCHES1_AUTO_CRC = (1 << 2);
static constexpr uint8_t FUSB_SWITCHES1_TXCC2 = (1 << 1);
static constexpr uint8_t FUSB_SWITCHES1_TXCC1 = 1;

// CONTROL0 bits
static constexpr uint8_t FUSB_CONTROL0_TX_FLUSH = (1 << 6);
static constexpr uint8_t FUSB_CONTROL0_INT_MASK = (1 << 5);
static constexpr uint8_t FUSB_CONTROL0_HOST_CUR_SHIFT = 2;
static constexpr uint8_t FUSB_CONTROL0_HOST_CUR = (0x3 << FUSB_CONTROL0_HOST_CUR_SHIFT);
static constexpr uint8_t FUSB_CONTROL0_AUTO_PRE = (1 << 1);
static constexpr uint8_t FUSB_CONTROL0_TX_START = 1;

// CONTROL1 bits
static constexpr uint8_t FUSB_CONTROL1_ENSOP2DB = (1 << 6);
static constexpr uint8_t FUSB_CONTROL1_ENSOP1DB = (1 << 5);
static constexpr uint8_t FUSB_CONTROL1_BIST_MODE2 = (1 << 4);
static constexpr uint8_t FUSB_CONTROL1_RX_FLUSH = (1 << 2);
static constexpr uint8_t FUSB_CONTROL1_ENSOP2 = (1 << 1);
static constexpr uint8_t FUSB_CONTROL1_ENSOP1 = 1;

// CONTROL3 bits
static constexpr uint8_t FUSB_CONTROL3_SEND_HARD_RESET = (1 << 6);
static constexpr uint8_t FUSB_CONTROL3_BIST_TMODE = (1 << 5);
static constexpr uint8_t FUSB_CONTROL3_AUTO_HARDRESET = (1 << 4);
static constexpr uint8_t FUSB_CONTROL3_AUTO_SOFTRESET = (1 << 3);
static constexpr uint8_t FUSB_CONTROL3_N_RETRIES_SHIFT = 1;
static constexpr uint8_t FUSB_CONTROL3_N_RETRIES_MASK = (0x3 << FUSB_CONTROL3_N_RETRIES_SHIFT);
static constexpr uint8_t FUSB_CONTROL3_AUTO_RETRY = 1;

// POWER bits
static constexpr uint8_t PWR_INT_OSC = (0x01 << 3);
static constexpr uint8_t PWR_MEASURE = (0x01 << 2);
static constexpr uint8_t PWR_RECEIVER = (0x01 << 1);
static constexpr uint8_t PWR_BANDGAP = (0x01 << 0);

// RESET bits
static constexpr uint8_t FUSB_RESET_PD_RESET = (1 << 1);
static constexpr uint8_t FUSB_RESET_SW_RES = 1;

// MASKA bits
static constexpr uint8_t FUSB_MASKA_M_OCP_TEMP = (1 << 7);
static constexpr uint8_t FUSB_MASKA_M_TOGDONE = (1 << 6);
static constexpr uint8_t FUSB_MASKA_M_SOFTFAIL = (1 << 5);
static constexpr uint8_t FUSB_MASKA_M_RETRYFAIL = (1 << 4);
static constexpr uint8_t FUSB_MASKA_M_HARDSENT = (1 << 3);
static constexpr uint8_t FUSB_MASKA_M_TXSENT = (1 << 2);
static constexpr uint8_t FUSB_MASKA_M_SOFTRST = (1 << 1);
static constexpr uint8_t FUSB_MASKA_M_HARDRST = 1;

// MASKB bits
static constexpr uint8_t FUSB_MASKB_M_GCRCSENT = 1;

// STATUS0A bits
static constexpr uint8_t FUSB_STATUS0A_SOFTFAIL = (1 << 5);
static constexpr uint8_t FUSB_STATUS0A_RETRYFAIL = (1 << 4);
static constexpr uint8_t FUSB_STATUS0A_SOFTRST = (1 << 1);
static constexpr uint8_t FUSB_STATUS0A_HARDRST = 1;

// INTERRUPTA bits
static constexpr uint8_t FUSB_INTERRUPTA_I_OCP_TEMP = (1 << 7);
static constexpr uint8_t FUSB_INTERRUPTA_I_TOGDONE = (1 << 6);
static constexpr uint8_t FUSB_INTERRUPTA_I_SOFTFAIL = (1 << 5);
static constexpr uint8_t FUSB_INTERRUPTA_I_RETRYFAIL = (1 << 4);
static constexpr uint8_t FUSB_INTERRUPTA_I_HARDSENT = (1 << 3);
static constexpr uint8_t FUSB_INTERRUPTA_I_TXSENT = (1 << 2);
static constexpr uint8_t FUSB_INTERRUPTA_I_SOFTRST = (1 << 1);
static constexpr uint8_t FUSB_INTERRUPTA_I_HARDRST = 1;

// INTERRUPTB bits
static constexpr uint8_t FUSB_INTERRUPTB_I_GCRCSENT = 1;

// STATUS0 bits
static constexpr uint8_t FUSB_STATUS0_VBUSOK = (1 << 7);
static constexpr uint8_t FUSB_STATUS0_ACTIVITY = (1 << 6);
static constexpr uint8_t FUSB_STATUS0_COMP = (1 << 5);
static constexpr uint8_t FUSB_STATUS0_CRC_CHK = (1 << 4);
static constexpr uint8_t FUSB_STATUS0_ALERT = (1 << 3);
static constexpr uint8_t FUSB_STATUS0_WAKE = (1 << 2);
static constexpr uint8_t FUSB_STATUS0_BC_LVL_SHIFT = 0;
static constexpr uint8_t FUSB_STATUS0_BC_LVL = (0x3 << FUSB_STATUS0_BC_LVL_SHIFT);

// STATUS1 bits
static constexpr uint8_t FUSB_STATUS1_RXSOP2 = (1 << 7);
static constexpr uint8_t FUSB_STATUS1_RXSOP1 = (1 << 6);
static constexpr uint8_t FUSB_STATUS1_RX_EMPTY = (1 << 5);
static constexpr uint8_t FUSB_STATUS1_RX_FULL = (1 << 4);
static constexpr uint8_t FUSB_STATUS1_TX_EMPTY = (1 << 3);
static constexpr uint8_t FUSB_STATUS1_TX_FULL = (1 << 2);
static constexpr uint8_t FUSB_STATUS1_OVRTEMP = (1 << 1);
static constexpr uint8_t FUSB_STATUS1_OCP = 1;

// FIFO RX token bits
static constexpr uint8_t FUSB_FIFO_RX_TOKEN_BITS = 0xE0;
static constexpr uint8_t FUSB_FIFO_RX_SOP = 0xE0;
static constexpr uint8_t FUSB_FIFO_RX_SOP1 = 0xC0;
static constexpr uint8_t FUSB_FIFO_RX_SOP2 = 0xA0;
static constexpr uint8_t FUSB_FIFO_RX_SOP1DB = 0x80;
static constexpr uint8_t FUSB_FIFO_RX_SOP2DB = 0x60;

}  // namespace fusb302b
}  // namespace esphome
