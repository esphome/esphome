#pragma once

#include "esphome/components/canbus/canbus.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/gpio.h"
#include "mcp251xfd_defs.h"

namespace esphome::mcp251xfd {

class MCP251XFD : public canbus::Canbus,
                  public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                        spi::DATA_RATE_8MHZ> {
 public:
  MCP251XFD(CanClock clock, CanMode mode, uint32_t nominal_bit_timing, uint32_t data_bit_timing,
            uint32_t transmitter_delay_compensation)
      : nominal_bit_timing_(nominal_bit_timing),
        data_bit_timing_(data_bit_timing),
        transmitter_delay_compensation_(transmitter_delay_compensation),
        mcp_clock_(clock),
        mcp_mode_(mode) {}

  void dump_config() override;
  // Read as an RX-pending level to skip the FIFO status read when idle; it does not wake the loop.
  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  canbus::CanStatus get_status() override;

 protected:
  InternalGPIOPin *interrupt_pin_{nullptr};
  uint32_t nominal_bit_timing_;
  uint32_t data_bit_timing_;
  uint32_t transmitter_delay_compensation_;
  uint32_t last_trec_{0};
  // Start one interval back so the first fault logs and the first mode check runs at once.
  uint32_t last_fault_log_time_{0u - FAULT_LOG_INTERVAL_MS};
  uint32_t last_mode_check_time_{0u - MODE_RECOVERY_INTERVAL_MS};
  // Latched flags are polled, so these count observations rather than exact losses.
  uint32_t rx_overrun_count_{0};
  uint32_t bus_error_count_{0};
  CanClock mcp_clock_;
  CanMode mcp_mode_;
  uint8_t last_error_flags_{0};
  bool comm_fault_active_{false};
  bool mode_fault_active_{false};

  bool setup_internal() override;
  canbus::CanEventFlags get_events() override;
  canbus::Error send_message(canbus::CanFrame *frame) override;
  canbus::Error read_message(canbus::CanFrame *frame) override;

  // Resets and configures the chip; SPI must be registered.
  bool configure_();
  bool reset_();
  bool configure_clock_();
  bool set_mode_(CanMode mode, uint16_t timeout_ms = 100);
  bool wait_for_register_(uint16_t address, uint8_t mask, uint8_t expected, uint16_t timeout_ms = 100);
  bool read_registers_(uint16_t address, uint8_t *data, size_t length);
  bool read_register_(uint16_t address, uint32_t &value);
  bool set_comm_fault_(const LogString *reason);
  bool should_log_fault_(uint32_t now);
  bool recover_mode_if_needed_(bool bus_off, bool retry_due);
  // Writes length bytes that follow two free bytes at transfer, which hold the command.
  void write_transfer_(uint16_t address, uint8_t *transfer, size_t length);
  void write_registers_(uint16_t address, const uint8_t *data, size_t length);
  void write_register_(uint16_t address, uint32_t value);
  void write_byte_(uint16_t address, uint8_t value);
  bool validate_fifo_address_(uint32_t offset, uint32_t start, uint32_t depth, uint16_t &address);
  bool classic_() const {
#ifndef USE_CANBUS_FD
    // Only CLASSIC buses leave CAN FD off
    return true;
#else
    return this->mcp_mode_ == CanMode::CAN_MODE_CLASSIC;
#endif
  }
  uint32_t message_size_() const { return this->classic_() ? CLASSIC_MESSAGE_SIZE : MESSAGE_SIZE; }
  uint32_t rx_fifo_depth_() const { return this->classic_() ? CLASSIC_RX_FIFO_DEPTH : RX_FIFO_DEPTH; }
};

}  // namespace esphome::mcp251xfd
