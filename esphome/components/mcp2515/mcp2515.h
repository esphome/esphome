#pragma once

#include "esphome/components/canbus/canbus.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"
#include "mcp2515_defs.h"

namespace esphome {
namespace mcp2515 {
static const uint32_t SPI_CLOCK = 10000000;  // 10MHz

static const int N_TXBUFFERS = 3;
static const int N_RXBUFFERS = 2;
enum CAN_CLOCK { MCP_20MHZ, MCP_16MHZ, MCP_8MHZ };
enum MASK { MASK0, MASK1 };
enum RXF { RXF0 = 0, RXF1 = 1, RXF2 = 2, RXF3 = 3, RXF4 = 4, RXF5 = 5 };
enum RXBn { RXB0 = 0, RXB1 = 1 };
enum TXBn { TXB0 = 0, TXB1 = 1, TXB2 = 2 };

enum CAN_CLKOUT {
  CLKOUT_DISABLE = -1,
  CLKOUT_DIV1 = 0x0,
  CLKOUT_DIV2 = 0x1,
  CLKOUT_DIV4 = 0x2,
  CLKOUT_DIV8 = 0x3,
};

enum /*class*/ CANINTF : uint8_t {
  CANINTF_RX0IF = 0x01,
  CANINTF_RX1IF = 0x02,
  CANINTF_TX0IF = 0x04,
  CANINTF_TX1IF = 0x08,
  CANINTF_TX2IF = 0x10,
  CANINTF_ERRIF = 0x20,
  CANINTF_WAKIF = 0x40,
  CANINTF_MERRF = 0x80
};

enum /*class*/ EFLG : uint8_t {
  EFLG_RX1OVR = (1 << 7),
  EFLG_RX0OVR = (1 << 6),
  EFLG_TXBO = (1 << 5),
  EFLG_TXEP = (1 << 4),
  EFLG_RXEP = (1 << 3),
  EFLG_TXWAR = (1 << 2),
  EFLG_RXWAR = (1 << 1),
  EFLG_EWARN = (1 << 0)
};

enum /*class*/ STAT : uint8_t { STAT_RX0IF = (1 << 0), STAT_RX1IF = (1 << 1) };

static const uint8_t STAT_RXIF_MASK = STAT_RX0IF | STAT_RX1IF;
static const uint8_t EFLG_ERRORMASK = EFLG_RX1OVR | EFLG_RX0OVR | EFLG_TXBO | EFLG_TXEP | EFLG_RXEP;

class MCP2515 : public canbus::Canbus,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                      spi::DATA_RATE_8MHZ> {
 public:
  MCP2515(){};

  static const struct TXBn_REGS {
    REGISTER CTRL;
    REGISTER SIDH;
    REGISTER DATA;
  } TXB[N_TXBUFFERS];

  static const struct RXBn_REGS {
    REGISTER CTRL;
    REGISTER SIDH;
    REGISTER DATA;
    CANINTF CANINTF_RXnIF;
  } RXB[N_RXBUFFERS];

 protected:
  bool setup_internal_() override;
  canbus::ERROR set_mode_(const CANCTRL_REQOP_MODE mode);

  uint8_t read_register_(const REGISTER reg);
  void read_registers_(const REGISTER reg, uint8_t values[], const uint8_t n);
  void set_register_(const REGISTER reg, const uint8_t value);
  void set_registers_(const REGISTER reg, uint8_t values[], const uint8_t n);
  void modify_register_(const REGISTER reg, const uint8_t mask, const uint8_t data);

  void prepare_id_(uint8_t *buffer, const bool ext, const uint32_t id);
  canbus::ERROR reset_(void);
  canbus::ERROR set_config_mode_();
  canbus::ERROR set_listen_only_();
  canbus::ERROR set_sleep_mode_();
  canbus::ERROR set_loop_back_mode_();
  canbus::ERROR set_normal_mode_();
  canbus::ERROR set_clk_out_(const CAN_CLKOUT divisor);
  canbus::ERROR set_bitrate_(canbus::CAN_SPEED can_speed);
  canbus::ERROR set_bitrate_(canbus::CAN_SPEED can_speed, const CAN_CLOCK can_clock);
  canbus::ERROR set_filter_mask_(const MASK num, const bool ext, const uint32_t ulData);
  canbus::ERROR set_filter_(const RXF num, const bool ext, const uint32_t ulData);
  canbus::ERROR send_message_(const TXBn txbn, const struct canbus::can_frame *frame);
  canbus::ERROR send_message_(const struct canbus::can_frame *frame);
  canbus::ERROR read_message_(const RXBn rxbn, struct canbus::can_frame *frame);
  canbus::ERROR read_message_(struct canbus::can_frame *frame);
  bool check_receive_(void);
  bool check_error_(void);
  uint8_t get_error_flags_(void);
  void clearRXnOVRFlags(void);
  uint8_t getInterrupts(void);
  uint8_t getInterruptMask(void);
  void clearInterrupts(void);
  void clearTXInterrupts(void);
  uint8_t get_status_(void);
  void clearRXnOVR(void);
  void clearMERR();
  void clearERRIF();
};
}  // namespace mcp2515
}  // namespace esphome