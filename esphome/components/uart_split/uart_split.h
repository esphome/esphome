#pragma once

#include "esphome/components/uart/uart_virtual.h"
#include "esphome/core/component.h"

#include <cstddef>
#include <cstdint>

namespace esphome::uart_split {

static constexpr size_t MAX_OUTPUTS = 8;
static constexpr size_t RX_BUFFER_SIZE = 256;

class UartSplit;

/// One consumer of a shared UART. Writes go to the pins unless this output is receive-only.
class UartSplitOutput final : public uart::VirtualUARTComponent {
 public:
  explicit UartSplitOutput(UartSplit *split) : VirtualUARTComponent(RX_BUFFER_SIZE), split_(split) {}
  void set_rx_only(bool rx_only) { this->rx_only_ = rx_only; }
  void set_mirror_tx(bool mirror_tx) { this->mirror_tx_ = mirror_tx; }

  bool rx_only() const { return this->rx_only_; }
  bool mirror_tx() const { return this->mirror_tx_; }
  /// A writer holds the read back while it is full, unless it has dropped bytes since it was last empty.
  bool holds_back() const { return !this->rx_only_ && !this->dropping_; }
  size_t rx_free() const { return static_cast<size_t>(this->rx_.capacity() - this->rx_.size()); }
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
  /// Take the baud rate, data bits, parity and stop bits of the UART that is split.
  void copy_settings();

  void write_array(const uint8_t *data, size_t len) override;
  size_t available_for_write() override;
  uart::UARTFlushResult flush() override;
  bool is_connected() override;
#if defined(USE_ESP8266) || defined(USE_ESP32)
  using VirtualUARTComponent::load_settings;
  void load_settings(bool dump_config) override;
#endif

 protected:
  UartSplit *split_;
  bool rx_only_{false};
  bool mirror_tx_{false};
  bool dropping_{false};
  bool write_drop_logged_{false};
#if defined(USE_ESP8266) || defined(USE_ESP32)
  bool load_settings_warned_{false};
#endif
};

/// The only reader of one hardware UART. Each output gets its own copy.
class UartSplit final : public Component {
 public:
  explicit UartSplit(uart::UARTComponent *parent) : parent_(parent) {}
  void add_output(UartSplitOutput *output);
  uart::UARTComponent *parent() const { return this->parent_; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  // After the hardware UART, before the devices on the outputs, which read the settings.
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
