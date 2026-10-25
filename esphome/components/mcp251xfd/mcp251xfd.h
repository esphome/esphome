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
  // Hardware flags can remain latched across several frames, so these count observations rather than exact losses.
  struct FaultCounters {
    uint32_t system_errors{0};
    uint32_t receive_overruns{0};
    uint32_t can_errors{0};
    uint32_t invalid_messages{0};
    uint32_t transmit_attempts_exhausted{0};
    uint32_t spi_crc_errors{0};
    uint32_t spi_format_errors{0};
    uint32_t host_crc_failures{0};
    uint32_t bus_off_events{0};
    uint32_t error_passive_events{0};
    uint32_t unexpected_mode_changes{0};
    uint32_t mode_recovery_requests{0};
  };

  MCP251XFD(CanClock clock, CanMode mode, uint32_t nominal_bit_timing, uint32_t data_bit_timing,
            uint32_t transmitter_delay_compensation)
      : mcp_clock_(clock),
        mcp_mode_(mode),
        nominal_bit_timing_(nominal_bit_timing),
        data_bit_timing_(data_bit_timing),
        transmitter_delay_compensation_(transmitter_delay_compensation) {}

  void dump_config() override;
  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  canbus::CanStatus get_status() override;
  const FaultCounters &get_fault_counters() const { return this->fault_counters_; }
  bool supports_fd() const override { return this->mcp_mode_ != CanMode::CAN_MODE_CLASSIC; }

 protected:
  CanClock mcp_clock_;
  CanMode mcp_mode_;
  uint32_t nominal_bit_timing_;
  uint32_t data_bit_timing_;
  uint32_t transmitter_delay_compensation_;
  uint32_t last_trec_{0};
  uint8_t last_error_flags_{0};
  FaultCounters fault_counters_{};
  bool spi_error_{false};
  uint32_t last_fault_log_time_{0};
  uint32_t last_mode_recovery_time_{0};
  bool fault_log_started_{false};
  bool mode_recovery_attempted_{false};
  bool mode_fault_active_{false};
  bool mode_recovery_pending_{false};
  InternalGPIOPin *interrupt_pin_{nullptr};

  // Keep this inline to avoid RP2 RAM section conflicts with inline core helpers.
  static void IRAM_ATTR gpio_intr(MCP251XFD *arg) { arg->enable_loop_soon_any_context(); }

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
  bool should_log_fault_(uint32_t now);
  void recover_mode_if_needed_(bool bus_off, uint32_t now);
  void write_registers_(uint16_t address, const uint8_t *data, size_t length);
  void write_register_(uint16_t address, uint32_t value);
  void write_byte_(uint16_t address, uint8_t value);
  bool read_fifo_address_(uint16_t reg, uint32_t start, uint32_t size, uint16_t &address);
};

}  // namespace esphome::mcp251xfd
