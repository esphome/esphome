#include "mcp2515.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp2515 {

static const char *TAG = "mcp2515";

const struct MCP2515::TxBnRegs MCP2515::TXB[N_TXBUFFERS] = {{MCP_TXB0CTRL, MCP_TXB0SIDH, MCP_TXB0DATA},
                                                            {MCP_TXB1CTRL, MCP_TXB1SIDH, MCP_TXB1DATA},
                                                            {MCP_TXB2CTRL, MCP_TXB2SIDH, MCP_TXB2DATA}};

const struct MCP2515::RxBnRegs MCP2515::RXB[N_RXBUFFERS] = {{MCP_RXB0CTRL, MCP_RXB0SIDH, MCP_RXB0DATA, CANINTF_RX0IF},
                                                            {MCP_RXB1CTRL, MCP_RXB1SIDH, MCP_RXB1DATA, CANINTF_RX1IF}};

bool MCP2515::setup_internal_() {
  ESP_LOGD(TAG, "setup_internal_()");
  this->spi_setup();

  if (this->reset_() == canbus::ERROR_FAIL)
    return false;
  this->set_bitrate_(this->bit_rate_, this->mcp_clock_);
  this->set_mode_(this->mcp_mode_);
  ESP_LOGD(TAG, "setup done send test message");
  return true;
}

canbus::Error MCP2515::reset_(void) {
  this->enable();
  this->transfer_byte(INSTRUCTION_RESET);
  this->disable();
  ESP_LOGD(TAG, "reset_()");
  delay(10);

  ESP_LOGD(TAG, "reset() CLEAR ALL TXB registers");

  uint8_t zeros[14];
  memset(zeros, 0, sizeof(zeros));
  set_registers_(MCP_TXB0CTRL, zeros, 14);
  set_registers_(MCP_TXB1CTRL, zeros, 14);
  set_registers_(MCP_TXB2CTRL, zeros, 14);
  ESP_LOGD(TAG, "reset() CLEARED TXB registers");

  set_register_(MCP_RXB0CTRL, 0);
  set_register_(MCP_RXB1CTRL, 0);

  set_register_(MCP_CANINTE, CANINTF_RX0IF | CANINTF_RX1IF | CANINTF_ERRIF | CANINTF_MERRF);

  modify_register_(MCP_RXB0CTRL, RXBnCTRL_RXM_MASK | RXB0CTRL_BUKT, RXBnCTRL_RXM_STDEXT | RXB0CTRL_BUKT);
  modify_register_(MCP_RXB1CTRL, RXBnCTRL_RXM_MASK, RXBnCTRL_RXM_STDEXT);

  return canbus::ERROR_OK;
}

uint8_t MCP2515::read_register_(const REGISTER reg) {
  this->enable();
  this->transfer_byte(INSTRUCTION_READ);
  this->transfer_byte(reg);
  uint8_t ret = this->transfer_byte(0x00);
  this->disable();

  return ret;
}

void MCP2515::read_registers_(const REGISTER reg, uint8_t values[], const uint8_t n) {
  this->enable();
  this->transfer_byte(INSTRUCTION_READ);
  this->transfer_byte(reg);
  // this->transfer_array(values, n);
  // mcp2515 has auto - increment of address - pointer
  for (uint8_t i = 0; i < n; i++) {
    values[i] = this->transfer_byte(0x00);
  }
  this->disable();
}

void MCP2515::set_register_(const REGISTER reg, const uint8_t value) {
  this->enable();
  this->transfer_byte(INSTRUCTION_WRITE);
  this->transfer_byte(reg);
  this->transfer_byte(value);
  this->disable();
}

void MCP2515::set_registers_(const REGISTER reg, uint8_t values[], const uint8_t n) {
  this->enable();
  this->transfer_byte(INSTRUCTION_WRITE);
  this->transfer_byte(reg);
  // this->transfer_array(values, n);
  for (uint8_t i = 0; i < n; i++) {
    this->transfer_byte(values[i]);
  }
  this->disable();
}

void MCP2515::modify_register_(const REGISTER reg, const uint8_t mask, const uint8_t data) {
  this->enable();
  this->transfer_byte(INSTRUCTION_BITMOD);
  this->transfer_byte(reg);
  this->transfer_byte(mask);
  this->transfer_byte(data);
  this->disable();
}

uint8_t MCP2515::get_status_(void) {
  this->enable();
  this->transfer_byte(INSTRUCTION_READ_STATUS);
  uint8_t i = this->transfer_byte(0x00);
  this->disable();

  return i;
}

canbus::Error MCP2515::set_mode_(const CANCTRL_REQOP_MODE mode) {
  modify_register_(MCP_CANCTRL, CANCTRL_REQOP, mode);

  unsigned long endTime = millis() + 10;
  bool modeMatch = false;
  while (millis() < endTime) {
    uint8_t newmode = read_register_(MCP_CANSTAT);
    newmode &= CANSTAT_OPMOD;
    modeMatch = newmode == mode;
    if (modeMatch) {
      break;
    }
  }
  return modeMatch ? canbus::ERROR_OK : canbus::ERROR_FAIL;
}

canbus::Error MCP2515::set_clk_out_(const CanClkOut divisor) {
  canbus::Error res;
  uint8_t cfg3;

  if (divisor == CLKOUT_DISABLE) {
    /* Turn off CLKEN */
    modify_register_(MCP_CANCTRL, CANCTRL_CLKEN, 0x00);

    /* Turn on CLKOUT for SOF */
    modify_register_(MCP_CNF3, CNF3_SOF, CNF3_SOF);
    return canbus::ERROR_OK;
  }

  /* Set the prescaler (CLKPRE) */
  modify_register_(MCP_CANCTRL, CANCTRL_CLKPRE, divisor);

  /* Turn on CLKEN */
  modify_register_(MCP_CANCTRL, CANCTRL_CLKEN, CANCTRL_CLKEN);

  /* Turn off CLKOUT for SOF */
  modify_register_(MCP_CNF3, CNF3_SOF, 0x00);
  return canbus::ERROR_OK;
}

void MCP2515::prepare_id_(uint8_t *buffer, const bool ext, const uint32_t id) {
  uint16_t canid = (uint16_t)(id & 0x0FFFF);

  if (ext) {
    buffer[MCP_EID0] = (uint8_t)(canid & 0xFF);
    buffer[MCP_EID8] = (uint8_t)(canid >> 8);
    canid = (uint16_t)(id >> 16);
    buffer[MCP_SIDL] = (uint8_t)(canid & 0x03);
    buffer[MCP_SIDL] += (uint8_t)((canid & 0x1C) << 3);
    buffer[MCP_SIDL] |= TXB_EXIDE_MASK;
    buffer[MCP_SIDH] = (uint8_t)(canid >> 5);
  } else {
    buffer[MCP_SIDH] = (uint8_t)(canid >> 3);
    buffer[MCP_SIDL] = (uint8_t)((canid & 0x07) << 5);
    buffer[MCP_EID0] = 0;
    buffer[MCP_EID8] = 0;
  }
}

canbus::Error MCP2515::set_filter_mask_(const MASK mask, const bool ext, const uint32_t ul_data) {
  canbus::Error res = set_mode_(CANCTRL_REQOP_CONFIG);
  if (res != canbus::ERROR_OK) {
    return res;
  }

  uint8_t tbufdata[4];
  prepare_id_(tbufdata, ext, ul_data);

  REGISTER reg;
  switch (mask) {
    case MASK0:
      reg = MCP_RXM0SIDH;
      break;
    case MASK1:
      reg = MCP_RXM1SIDH;
      break;
    default:
      return canbus::ERROR_FAIL;
  }

  set_registers_(reg, tbufdata, 4);

  return canbus::ERROR_OK;
}

canbus::Error MCP2515::set_filter_(const RXF num, const bool ext, const uint32_t ul_data) {
  canbus::Error res = set_mode_(CANCTRL_REQOP_CONFIG);
  if (res != canbus::ERROR_OK) {
    return res;
  }

  REGISTER reg;

  switch (num) {
    case RXF0:
      reg = MCP_RXF0SIDH;
      break;
    case RXF1:
      reg = MCP_RXF1SIDH;
      break;
    case RXF2:
      reg = MCP_RXF2SIDH;
      break;
    case RXF3:
      reg = MCP_RXF3SIDH;
      break;
    case RXF4:
      reg = MCP_RXF4SIDH;
      break;
    case RXF5:
      reg = MCP_RXF5SIDH;
      break;
    default:
      return canbus::ERROR_FAIL;
  }

  uint8_t tbufdata[4];
  prepare_id_(tbufdata, ext, ul_data);
  set_registers_(reg, tbufdata, 4);

  return canbus::ERROR_OK;
}

canbus::Error MCP2515::send_message_(const TXBn txbn, const struct canbus::CanFrame *frame) {
  const struct TxBnRegs *txbuf = &TXB[txbn];

  uint8_t data[13];

  bool ext = (frame->can_id & canbus::CAN_EFF_FLAG);
  bool rtr = (frame->can_id & canbus::CAN_RTR_FLAG);
  uint32_t id = (frame->can_id & (ext ? canbus::CAN_EFF_MASK : canbus::CAN_SFF_MASK));
  prepare_id_(data, ext, id);
  data[MCP_DLC] = rtr ? (frame->can_dlc | RTR_MASK) : frame->can_dlc;
  memcpy(&data[MCP_DATA], frame->data, frame->can_dlc);
  set_registers_(txbuf->SIDH, data, 5 + frame->can_dlc);
  modify_register_(txbuf->CTRL, TXB_TXREQ, TXB_TXREQ);

  return canbus::ERROR_OK;
}

canbus::Error MCP2515::send_message_(const struct canbus::CanFrame *frame) {
  // ESP_LOGD(TAG, "send_message_: frame.id = %d", frame->can_id);
  if (frame->can_dlc > canbus::CAN_MAX_DLEN) {
    return canbus::ERROR_FAILTX;
  }
  // ESP_LOGD(TAG, "send_message_: size = %d is OK", frame->can_dlc);
  TXBn txBuffers[N_TXBUFFERS] = {TXB0, TXB1, TXB2};

  for (int i = 0; i < N_TXBUFFERS; i++) {
    const struct TxBnRegs *txbuf = &TXB[txBuffers[i]];
    uint8_t ctrlval = read_register_(txbuf->CTRL);
    if ((ctrlval & TXB_TXREQ) == 0) {
      // ESP_LOGD(TAG, "send buffer: %d,  ctrl_val = %d", i, ctrlval);
      return send_message_(txBuffers[i], frame);
    }
  }

  return canbus::ERROR_FAILTX;
}

canbus::Error MCP2515::read_message_(const RXBn rxbn, struct canbus::CanFrame *frame) {
  const struct RxBnRegs *rxb = &RXB[rxbn];

  uint8_t tbufdata[5];

  read_registers_(rxb->SIDH, tbufdata, 5);

  uint32_t id = (tbufdata[MCP_SIDH] << 3) + (tbufdata[MCP_SIDL] >> 5);

  if ((tbufdata[MCP_SIDL] & TXB_EXIDE_MASK) == TXB_EXIDE_MASK) {
    id = (id << 2) + (tbufdata[MCP_SIDL] & 0x03);
    id = (id << 8) + tbufdata[MCP_EID8];
    id = (id << 8) + tbufdata[MCP_EID0];
    id |= canbus::CAN_EFF_FLAG;
  }

  uint8_t dlc = (tbufdata[MCP_DLC] & DLC_MASK);
  if (dlc > canbus::CAN_MAX_DLEN) {
    return canbus::ERROR_FAIL;
  }

  uint8_t ctrl = read_register_(rxb->CTRL);
  if (ctrl & RXBnCTRL_RTR) {
    id |= canbus::CAN_RTR_FLAG;
  }

  frame->can_id = id;
  frame->can_dlc = dlc;

  read_registers_(rxb->DATA, frame->data, dlc);

  modify_register_(MCP_CANINTF, rxb->CANINTF_RXnIF, 0);

  return canbus::ERROR_OK;
}

canbus::Error MCP2515::read_message_(struct canbus::CanFrame *frame) {
  canbus::Error rc;
  uint8_t stat = get_status_();

  if (stat & STAT_RX0IF) {
    rc = read_message_(RXB0, frame);
  } else if (stat & STAT_RX1IF) {
    rc = read_message_(RXB1, frame);
  } else {
    rc = canbus::ERROR_NOMSG;
  }

  return rc;
}

bool MCP2515::check_receive_(void) {
  uint8_t res = get_status_();
  if (res & STAT_RXIF_MASK) {
    return true;
  } else {
    return false;
  }
}

bool MCP2515::check_error_(void) {
  uint8_t eflg = get_error_flags_();

  if (eflg & EFLG_ERRORMASK) {
    return true;
  } else {
    return false;
  }
}

uint8_t MCP2515::get_error_flags_(void) { return read_register_(MCP_EFLG); }

void MCP2515::clear_rx_n_ovr_flags_(void) { modify_register_(MCP_EFLG, EFLG_RX0OVR | EFLG_RX1OVR, 0); }

uint8_t MCP2515::get_int_(void) { return read_register_(MCP_CANINTF); }

void MCP2515::clear_int_(void) { set_register_(MCP_CANINTF, 0); }

uint8_t MCP2515::get_int_mask_(void) { return read_register_(MCP_CANINTE); }

void MCP2515::clear_tx_int_(void) { modify_register_(MCP_CANINTF, (CANINTF_TX0IF | CANINTF_TX1IF | CANINTF_TX2IF), 0); }

void MCP2515::clear_rx_n_ovr_(void) {
  uint8_t eflg = get_error_flags_();
  if (eflg != 0) {
    clear_rx_n_ovr_flags_();
    clear_int_();
    // modify_register_(MCP_CANINTF, CANINTF_ERRIF, 0);
  }
}

void MCP2515::clear_merr_() {
  // modify_register_(MCP_EFLG, EFLG_RX0OVR | EFLG_RX1OVR, 0);
  // clear_int_();
  modify_register_(MCP_CANINTF, CANINTF_MERRF, 0);
}

void MCP2515::clear_errif_() {
  // modify_register_(MCP_EFLG, EFLG_RX0OVR | EFLG_RX1OVR, 0);
  // clear_int_();
  modify_register_(MCP_CANINTF, CANINTF_ERRIF, 0);
}

canbus::Error MCP2515::set_bitrate_(canbus::CanSpeed can_speed) { return this->set_bitrate_(can_speed, MCP_16MHZ); }

canbus::Error MCP2515::set_bitrate_(canbus::CanSpeed can_speed, CanClock can_clock) {
  canbus::Error error = set_mode_(CANCTRL_REQOP_CONFIG);
  if (error != canbus::ERROR_OK) {
    return error;
  }

  uint8_t set, cfg1, cfg2, cfg3;
  set = 1;
  switch (can_clock) {
    case (MCP_8MHZ):
      switch (can_speed) {
        case (canbus::CAN_5KBPS):  //   5KBPS
          cfg1 = MCP_8MHz_5kBPS_CFG1;
          cfg2 = MCP_8MHz_5kBPS_CFG2;
          cfg3 = MCP_8MHz_5kBPS_CFG3;
          break;
        case (canbus::CAN_10KBPS):  //  10KBPS
          cfg1 = MCP_8MHz_10kBPS_CFG1;
          cfg2 = MCP_8MHz_10kBPS_CFG2;
          cfg3 = MCP_8MHz_10kBPS_CFG3;
          break;
        case (canbus::CAN_20KBPS):  //  20KBPS
          cfg1 = MCP_8MHz_20kBPS_CFG1;
          cfg2 = MCP_8MHz_20kBPS_CFG2;
          cfg3 = MCP_8MHz_20kBPS_CFG3;
          break;
        case (canbus::CAN_31K25BPS):  //  31.25KBPS
          cfg1 = MCP_8MHz_31k25BPS_CFG1;
          cfg2 = MCP_8MHz_31k25BPS_CFG2;
          cfg3 = MCP_8MHz_31k25BPS_CFG3;
          break;
        case (canbus::CAN_33KBPS):  //  33.333KBPS
          cfg1 = MCP_8MHz_33k3BPS_CFG1;
          cfg2 = MCP_8MHz_33k3BPS_CFG2;
          cfg3 = MCP_8MHz_33k3BPS_CFG3;
          break;
        case (canbus::CAN_40KBPS):  //  40Kbps
          cfg1 = MCP_8MHz_40kBPS_CFG1;
          cfg2 = MCP_8MHz_40kBPS_CFG2;
          cfg3 = MCP_8MHz_40kBPS_CFG3;
          break;
        case (canbus::CAN_50KBPS):  //  50Kbps
          cfg1 = MCP_8MHz_50kBPS_CFG1;
          cfg2 = MCP_8MHz_50kBPS_CFG2;
          cfg3 = MCP_8MHz_50kBPS_CFG3;
          break;
        case (canbus::CAN_80KBPS):  //  80Kbps
          cfg1 = MCP_8MHz_80kBPS_CFG1;
          cfg2 = MCP_8MHz_80kBPS_CFG2;
          cfg3 = MCP_8MHz_80kBPS_CFG3;
          break;
        case (canbus::CAN_100KBPS):  // 100Kbps
          cfg1 = MCP_8MHz_100kBPS_CFG1;
          cfg2 = MCP_8MHz_100kBPS_CFG2;
          cfg3 = MCP_8MHz_100kBPS_CFG3;
          break;
        case (canbus::CAN_125KBPS):  // 125Kbps
          cfg1 = MCP_8MHz_125kBPS_CFG1;
          cfg2 = MCP_8MHz_125kBPS_CFG2;
          cfg3 = MCP_8MHz_125kBPS_CFG3;
          break;
        case (canbus::CAN_200KBPS):  // 200Kbps
          cfg1 = MCP_8MHz_200kBPS_CFG1;
          cfg2 = MCP_8MHz_200kBPS_CFG2;
          cfg3 = MCP_8MHz_200kBPS_CFG3;
          break;
        case (canbus::CAN_250KBPS):  // 250Kbps
          cfg1 = MCP_8MHz_250kBPS_CFG1;
          cfg2 = MCP_8MHz_250kBPS_CFG2;
          cfg3 = MCP_8MHz_250kBPS_CFG3;
          break;
        case (canbus::CAN_500KBPS):  // 500Kbps
          cfg1 = MCP_8MHz_500kBPS_CFG1;
          cfg2 = MCP_8MHz_500kBPS_CFG2;
          cfg3 = MCP_8MHz_500kBPS_CFG3;
          break;
        case (canbus::CAN_1000KBPS):  //   1Mbps
          cfg1 = MCP_8MHz_1000kBPS_CFG1;
          cfg2 = MCP_8MHz_1000kBPS_CFG2;
          cfg3 = MCP_8MHz_1000kBPS_CFG3;
          break;
        default:
          set = 0;
          break;
      }
      break;

    case (MCP_16MHZ):
      switch (can_speed) {
        case (canbus::CAN_5KBPS):  //   5Kbps
          cfg1 = MCP_16MHz_5kBPS_CFG1;
          cfg2 = MCP_16MHz_5kBPS_CFG2;
          cfg3 = MCP_16MHz_5kBPS_CFG3;
          break;
        case (canbus::CAN_10KBPS):  //  10Kbps
          cfg1 = MCP_16MHz_10kBPS_CFG1;
          cfg2 = MCP_16MHz_10kBPS_CFG2;
          cfg3 = MCP_16MHz_10kBPS_CFG3;
          break;
        case (canbus::CAN_20KBPS):  //  20Kbps
          cfg1 = MCP_16MHz_20kBPS_CFG1;
          cfg2 = MCP_16MHz_20kBPS_CFG2;
          cfg3 = MCP_16MHz_20kBPS_CFG3;
          break;
        case (canbus::CAN_33KBPS):  //  33.333Kbps
          cfg1 = MCP_16MHz_33k3BPS_CFG1;
          cfg2 = MCP_16MHz_33k3BPS_CFG2;
          cfg3 = MCP_16MHz_33k3BPS_CFG3;
          break;
        case (canbus::CAN_40KBPS):  //  40Kbps
          cfg1 = MCP_16MHz_40kBPS_CFG1;
          cfg2 = MCP_16MHz_40kBPS_CFG2;
          cfg3 = MCP_16MHz_40kBPS_CFG3;
          break;
        case (canbus::CAN_50KBPS):  //  50Kbps
          cfg2 = MCP_16MHz_50kBPS_CFG2;
          cfg3 = MCP_16MHz_50kBPS_CFG3;
          break;
        case (canbus::CAN_80KBPS):  //  80Kbps
          cfg1 = MCP_16MHz_80kBPS_CFG1;
          cfg2 = MCP_16MHz_80kBPS_CFG2;
          cfg3 = MCP_16MHz_80kBPS_CFG3;
          break;
        case (canbus::CAN_83K3BPS):  //  83.333Kbps
          cfg1 = MCP_16MHz_83k3BPS_CFG1;
          cfg2 = MCP_16MHz_83k3BPS_CFG2;
          cfg3 = MCP_16MHz_83k3BPS_CFG3;
          break;
        case (canbus::CAN_100KBPS):  // 100Kbps
          cfg1 = MCP_16MHz_100kBPS_CFG1;
          cfg2 = MCP_16MHz_100kBPS_CFG2;
          cfg3 = MCP_16MHz_100kBPS_CFG3;
          break;
        case (canbus::CAN_125KBPS):  // 125Kbps
          cfg1 = MCP_16MHz_125kBPS_CFG1;
          cfg2 = MCP_16MHz_125kBPS_CFG2;
          cfg3 = MCP_16MHz_125kBPS_CFG3;
          break;
        case (canbus::CAN_200KBPS):  // 200Kbps
          cfg1 = MCP_16MHz_200kBPS_CFG1;
          cfg2 = MCP_16MHz_200kBPS_CFG2;
          cfg3 = MCP_16MHz_200kBPS_CFG3;
          break;
        case (canbus::CAN_250KBPS):  // 250Kbps
          cfg1 = MCP_16MHz_250kBPS_CFG1;
          cfg2 = MCP_16MHz_250kBPS_CFG2;
          cfg3 = MCP_16MHz_250kBPS_CFG3;
          break;
        case (canbus::CAN_500KBPS):  // 500Kbps
          cfg1 = MCP_16MHz_500kBPS_CFG1;
          cfg2 = MCP_16MHz_500kBPS_CFG2;
          cfg3 = MCP_16MHz_500kBPS_CFG3;
          break;
        case (canbus::CAN_1000KBPS):  //   1Mbps
          cfg1 = MCP_16MHz_1000kBPS_CFG1;
          cfg2 = MCP_16MHz_1000kBPS_CFG2;
          cfg3 = MCP_16MHz_1000kBPS_CFG3;
          break;
        default:
          set = 0;
          break;
      }
      break;

    case (MCP_20MHZ):
      switch (can_speed) {
        case (canbus::CAN_33KBPS):  //  33.333Kbps
          cfg1 = MCP_20MHz_33k3BPS_CFG1;
          cfg2 = MCP_20MHz_33k3BPS_CFG2;
          cfg3 = MCP_20MHz_33k3BPS_CFG3;
          break;
        case (canbus::CAN_40KBPS):  //  40Kbps
          cfg1 = MCP_20MHz_40kBPS_CFG1;
          cfg2 = MCP_20MHz_40kBPS_CFG2;
          cfg3 = MCP_20MHz_40kBPS_CFG3;
          break;
        case (canbus::CAN_50KBPS):  //  50Kbps
          cfg1 = MCP_20MHz_50kBPS_CFG1;
          cfg2 = MCP_20MHz_50kBPS_CFG2;
          cfg3 = MCP_20MHz_50kBPS_CFG3;
          break;
        case (canbus::CAN_80KBPS):  //  80Kbps
          cfg1 = MCP_20MHz_80kBPS_CFG1;
          cfg2 = MCP_20MHz_80kBPS_CFG2;
          cfg3 = MCP_20MHz_80kBPS_CFG3;
          break;
        case (canbus::CAN_83K3BPS):  //  83.333Kbps
          cfg1 = MCP_20MHz_83k3BPS_CFG1;
          cfg2 = MCP_20MHz_83k3BPS_CFG2;
          cfg3 = MCP_20MHz_83k3BPS_CFG3;
          break;
        case (canbus::CAN_100KBPS):  // 100Kbps
          cfg1 = MCP_20MHz_100kBPS_CFG1;
          cfg2 = MCP_20MHz_100kBPS_CFG2;
          cfg3 = MCP_20MHz_100kBPS_CFG3;
          break;
        case (canbus::CAN_125KBPS):  // 125Kbps
          cfg1 = MCP_20MHz_125kBPS_CFG1;
          cfg2 = MCP_20MHz_125kBPS_CFG2;
          cfg3 = MCP_20MHz_125kBPS_CFG3;
          break;
        case (canbus::CAN_200KBPS):  // 200Kbps
          cfg1 = MCP_20MHz_200kBPS_CFG1;
          cfg2 = MCP_20MHz_200kBPS_CFG2;
          cfg3 = MCP_20MHz_200kBPS_CFG3;
          break;
        case (canbus::CAN_250KBPS):  // 250Kbps
          cfg1 = MCP_20MHz_250kBPS_CFG1;
          cfg2 = MCP_20MHz_250kBPS_CFG2;
          cfg3 = MCP_20MHz_250kBPS_CFG3;
          break;
        case (canbus::CAN_500KBPS):  // 500Kbps
          cfg1 = MCP_20MHz_500kBPS_CFG1;
          cfg2 = MCP_20MHz_500kBPS_CFG2;
          cfg3 = MCP_20MHz_500kBPS_CFG3;
          break;
        case (canbus::CAN_1000KBPS):  //   1Mbps
          cfg1 = MCP_20MHz_1000kBPS_CFG1;
          cfg2 = MCP_20MHz_1000kBPS_CFG2;
          cfg3 = MCP_20MHz_1000kBPS_CFG3;
          break;
        default:
          set = 0;
          break;
      }
      break;

    default:
      set = 0;
      break;
  }

  if (set) {
    set_register_(MCP_CNF1, cfg1);
    set_register_(MCP_CNF2, cfg2);
    set_register_(MCP_CNF3, cfg3);
    return canbus::ERROR_OK;
  } else {
    return canbus::ERROR_FAIL;
  }
}
}  // namespace mcp2515
}  // namespace esphome
