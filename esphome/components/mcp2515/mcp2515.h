#pragma once

#include "esphome/components/canbus/canbus.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"
#include "mcp2515_defs.h"

namespace esphome {
namespace mcp2515 {

class MCP2515 : public canbus::Canbus,
      public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                            spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_8MHZ> {
public:
  MCP2515(){};
  
  void set_cs_pin(GPIOPin *cs_pin) { cs_pin_ = cs_pin; }

  static const uint32_t SPI_CLOCK = 10000000; // 10MHz
  /* special address description flags for the CAN_ID */
  static const uint32_t CAN_EFF_FLAG =
      0x80000000UL; /* EFF/SFF is set in the MSB */
  static const uint32_t CAN_RTR_FLAG =
      0x40000000UL; /* remote transmission request */
  static const uint32_t CAN_ERR_FLAG = 0x20000000UL; /* error message frame */

  /* valid bits in CAN ID for frame formats */
  static const uint32_t CAN_SFF_MASK =
      0x000007FFUL; /* standard frame format (SFF) */
  static const uint32_t CAN_EFF_MASK =
      0x1FFFFFFFUL; /* extended frame format (EFF) */
  static const uint32_t CAN_ERR_MASK =
      0x1FFFFFFFUL; /* omit EFF, RTR, ERR flags */

  static const int N_TXBUFFERS = 3;
  static const int N_RXBUFFERS = 2;

/*
 * Controller Area Network Identifier structure
 *
 * bit 0-28 : CAN identifier (11/29 bit)
 * bit 29   : error message frame flag (0 = data frame, 1 = error message)
 * bit 30   : remote transmission request flag (1 = rtr frame)
 * bit 31   : frame format flag (0 = standard 11 bit, 1 = extended 29 bit)
 */
typedef uint32_t canid_t;

/* CAN payload length and DLC definitions according to ISO 11898-1 */
static const uint8_t CAN_MAX_DLC = 8;
static const uint8_t  CAN_MAX_DLEN =8;


struct can_frame {
    canid_t can_id;  /* 32 bit CAN_ID + EFF/RTR/ERR flags */
    uint8_t    can_dlc; /* frame payload length in byte (0 .. CAN_MAX_DLEN) */
    uint8_t    data[CAN_MAX_DLEN] __attribute__((aligned(8)));
};



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
  GPIOPin *cs_pin_;
  bool send_internal_(int can_id, uint8_t *data) override;
  bool setup_internal_() override;
        ERROR set_mode_(const CANCTRL_REQOP_MODE mode);

        uint8_t read_register_(const REGISTER reg);
        void read_registers_(const REGISTER reg, uint8_t values[], const uint8_t n);
        void set_register_(const REGISTER reg, const uint8_t value);
        void set_registers_(const REGISTER reg, uint8_t values[], const uint8_t n);
        void modify_register_(const REGISTER reg, const uint8_t mask, const uint8_t data);

        void prepare_id_(uint8_t *buffer, const bool ext, const uint32_t id);
        ERROR reset_(void);
        ERROR set_config_mode_();
        ERROR set_listen_only_();
        ERROR set_sleep_mode_();
        ERROR set_loop_back_mode_();
        ERROR set_normal_mode_();
        ERROR set_clk_out_(const CAN_CLKOUT divisor);
        ERROR set_bitrate_(const CAN_SPEED canSpeed) override;
        ERROR set_bitrate_(const CAN_SPEED canSpeed, const CAN_CLOCK canClock);
        ERROR set_filter_mask_(const MASK num, const bool ext, const uint32_t ulData);
        ERROR set_filter_(const RXF num, const bool ext, const uint32_t ulData);
        ERROR send_message_(const TXBn txbn, const struct can_frame *frame);
        ERROR send_message_(const struct can_frame *frame);
        ERROR readMessage(const RXBn rxbn, struct can_frame *frame);
        ERROR readMessage(struct can_frame *frame);
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
} // namespace mcp2515
} // namespace esphome