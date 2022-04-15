#pragma once

#include <vector>
#include <cstring>
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#ifdef USE_UART_DEBUGGER
#include "esphome/core/automation.h"
#endif

namespace esphome {
namespace uart {

enum UARTParityOptions {
  UART_CONFIG_PARITY_NONE,
  UART_CONFIG_PARITY_EVEN,
  UART_CONFIG_PARITY_ODD,
};

#ifdef USE_UART_DEBUGGER
enum UARTDirection {
  UART_DIRECTION_RX,
  UART_DIRECTION_TX,
  UART_DIRECTION_BOTH,
};
#endif

const LogString *parity_to_str(UARTParityOptions parity);

class UARTComponent {
 public:
  void write_array(const std::vector<uint8_t> &data) { this->write_array(&data[0], data.size()); }
  void write_byte(uint8_t data) { this->write_array(&data, 1); };
  void write_str(const char *str) {
    const auto *data = reinterpret_cast<const uint8_t *>(str);
    this->write_array(data, strlen(str));
  };

  virtual void write_array(const uint8_t *data, size_t len) = 0;

  bool read_byte(uint8_t *data) { return this->read_array(data, 1); };
  virtual bool peek_byte(uint8_t *data) = 0;
  virtual bool read_array(uint8_t *data, size_t len) = 0;

  /// Return available number of bytes.
  virtual int available() = 0;
  /// Block until all bytes have been written to the UART bus.
  virtual void flush() = 0;

  void set_tx_pin(InternalGPIOPin *tx_pin) { this->tx_pin_ = tx_pin; }
  void set_rx_pin(InternalGPIOPin *rx_pin) { this->rx_pin_ = rx_pin; }
  void set_rx_buffer_size(size_t rx_buffer_size) { this->rx_buffer_size_ = rx_buffer_size; }
  size_t get_rx_buffer_size() { return this->rx_buffer_size_; }

  void set_stop_bits(uint8_t stop_bits) { this->stop_bits_ = stop_bits; }
  uint8_t get_stop_bits() const { return this->stop_bits_; }
  void set_data_bits(uint8_t data_bits) { this->data_bits_ = data_bits; }
  uint8_t get_data_bits() const { return this->data_bits_; }
  void set_parity(UARTParityOptions parity) { this->parity_ = parity; }
  UARTParityOptions get_parity() const { return this->parity_; }
  void set_baud_rate(uint32_t baud_rate) { baud_rate_ = baud_rate; }
  uint32_t get_baud_rate() const { return baud_rate_; }

#ifdef USE_UART_DEBUGGER
  void add_debug_callback(std::function<void(UARTDirection, uint8_t)> &&callback) {
    this->debug_callback_.add(std::move(callback));
  }
#endif

 protected:
  virtual void check_logger_conflict() = 0;
  bool check_read_timeout_(size_t len = 1);

  InternalGPIOPin *tx_pin_;
  InternalGPIOPin *rx_pin_;
  size_t rx_buffer_size_;
  uint32_t baud_rate_;
  uint8_t stop_bits_;
  uint8_t data_bits_;
  UARTParityOptions parity_;
#ifdef USE_UART_DEBUGGER
  CallbackManager<void(UARTDirection, uint8_t)> debug_callback_{};
#endif
};

}  // namespace uart
}  // namespace esphome
