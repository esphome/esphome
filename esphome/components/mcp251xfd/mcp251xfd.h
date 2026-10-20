#pragma once

#include "esphome/components/canbus/canbus.h"
#include "esphome/components/spi/spi.h"
#include "mcp251xfd_defs.h"

namespace esphome::mcp251xfd {

class MCP251XFD : public canbus::Canbus,
                  public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                        spi::DATA_RATE_8MHZ> {
 public:
  MCP251XFD(CanClock clock, CanMode mode, uint32_t nominal_bit_timing, uint32_t data_bit_timing,
            uint32_t transmitter_delay_compensation)
      : mcp_clock_(clock),
        mcp_mode_(mode),
        nominal_bit_timing_(nominal_bit_timing),
        data_bit_timing_(data_bit_timing),
        transmitter_delay_compensation_(transmitter_delay_compensation) {}

  void dump_config() override;
  canbus::CanStatus get_status() override;
  bool supports_fd() const override { return this->mcp_mode_ != CanMode::CAN_MODE_CLASSIC; }

 protected:
  CanClock mcp_clock_;
  CanMode mcp_mode_;
  uint32_t nominal_bit_timing_;
  uint32_t data_bit_timing_;
  uint32_t transmitter_delay_compensation_;
  uint32_t last_trec_{0};
  uint8_t last_error_flags_{0};
  bool spi_error_{false};

  bool setup_internal() override;
  canbus::CanEventFlags get_events() override;
  canbus::Error send_message(canbus::CanFrame *frame) override;
  canbus::Error read_message(canbus::CanFrame *frame) override;

  bool reset_();
  bool configure_clock_();
  bool set_mode_(CanMode mode);
  bool wait_for_register_(uint16_t address, uint8_t mask, uint8_t expected);
  bool read_registers_(uint16_t address, uint8_t *data, size_t length);
  bool read_register_(uint16_t address, uint32_t &value);
  void write_registers_(uint16_t address, const uint8_t *data, size_t length);
  void write_register_(uint16_t address, uint32_t value);
  void write_byte_(uint16_t address, uint8_t value);
  bool read_fifo_address_(uint16_t reg, uint32_t start, uint32_t size, uint16_t &address);
};

}  // namespace esphome::mcp251xfd
