#pragma once

#include "esphome/components/uart/uart_component.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <cstddef>
#include <cstdint>

namespace esphome::uart_split {

static constexpr size_t MAX_OUTPUTS = 8;
static constexpr size_t RX_BUFFER_SIZE = 256;

class UartSplit;

/// One consumer of a shared UART. Bytes arrive in rx_; writes go to the pins
/// unless this output is receive-only.
class UartSplitOutput : public uart::UARTComponent {
 public:
  explicit UartSplitOutput(UartSplit *split) : split_(split) {}
  void set_rx_only(bool rx_only) { this->rx_only_ = rx_only; }
  void set_mirror_tx(bool mirror_tx) { this->mirror_tx_ = mirror_tx; }

  bool rx_only() const { return this->rx_only_; }
  bool mirror_tx() const { return this->mirror_tx_; }
  /// A writer holds the read back while it is full, unless it has dropped bytes since it was last empty.
  bool holds_back() const { return !this->rx_only_ && !this->dropping_; }
  size_t rx_free() const { return RX_BUFFER_SIZE - this->rx_.size(); }
  /// Copy bytes into the receive buffer. Returns false when some did not fit.
  bool push_rx(const uint8_t *data, size_t len);
  /// Mark that bytes were dropped. Returns true for the first drop since the buffer was last empty.
  bool start_dropping() {
    bool first = !this->dropping_;
    this->dropping_ = true;
    return first;
  }
  void end_dropping_if_empty() {
    if (this->rx_.empty()) {
      this->dropping_ = false;
    }
  }

  void write_array(const uint8_t *data, size_t len) override;
  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;
  size_t available() override { return this->rx_.size(); }
  size_t available_for_write() override;
  uart::UARTFlushResult flush() override;
  bool is_connected() override;
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif

 protected:
  void check_logger_conflict() override {}

  UartSplit *split_;
  bool rx_only_{false};
  bool mirror_tx_{false};
  bool dropping_{false};
  bool write_drop_logged_{false};
  StaticRingBuffer<uint8_t, RX_BUFFER_SIZE> rx_{};
};

/// The only reader of one hardware UART. Each output gets its own copy.
class UartSplit : public Component {
 public:
  explicit UartSplit(uart::UARTComponent *parent) : parent_(parent) {}
  void add_output(UartSplitOutput *output);
  uart::UARTComponent *parent() const { return this->parent_; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  // After the hardware UART (BUS), before the devices on the outputs (modbus is BUS - 1), which read the settings.
  float get_setup_priority() const override { return setup_priority::BUS - 0.5f; }

  /// Copy bytes a writer just sent into every other output that asked for them.
  void mirror_tx(const UartSplitOutput *from, const uint8_t *data, size_t len);

 protected:
  void push_(uint8_t index, const uint8_t *data, size_t len);

  uart::UARTComponent *parent_;
  UartSplitOutput *outputs_[MAX_OUTPUTS]{};
  uint8_t output_count_{0};
};

}  // namespace esphome::uart_split
