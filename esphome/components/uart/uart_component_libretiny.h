#pragma once

#ifdef USE_LIBRETINY

#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "uart_component.h"

namespace esphome::uart {

class LibreTinyUARTComponent final : public UARTComponent, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void load_settings(bool dump_config) override;
  using UARTComponent::load_settings;  // also bring in the no-arg overload for convenience
  float get_setup_priority() const override { return setup_priority::BUS; }

  void write_array(const uint8_t *data, size_t len) override;

  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;

  size_t available() override;
  UARTFlushResult flush() override;

  uint16_t get_config();
  /// Hardware port codegen picked from the board's pin lists.
  void set_hardware_port(int8_t port) { this->hardware_idx_ = port; }

  HardwareSerial *get_hw_serial() { return this->serial_; }
  int8_t get_hw_serial_number() { return this->hardware_idx_; }

#ifdef USE_UART_TX_BREAK
  bool set_tx_break(bool enable) override;
#endif

 protected:
  void check_logger_conflict() override;
  /// Opens the hardware port of hardware_idx_ on the given pins (PIN_INVALID = none), at setup and after a break.
#ifdef USE_UART_TX_BREAK
  void open_hardware_port_(pin_size_t tx_pin, pin_size_t rx_pin);
#else
  // one caller without the break: inlined, the build keeps its size
  ESPHOME_ALWAYS_INLINE inline void open_hardware_port_(pin_size_t tx_pin, pin_size_t rx_pin);
#endif

  HardwareSerial *serial_{nullptr};
  int8_t hardware_idx_{-1};
#ifdef USE_UART_TX_BREAK
  bool tx_break_{false};
#endif
};

}  // namespace esphome::uart
#endif  // USE_LIBRETINY
