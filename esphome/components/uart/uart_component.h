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
  // Writes an array of bytes to the UART bus.
  // @param data A vector of bytes to be written.
  void write_array(const std::vector<uint8_t> &data) { this->write_array(&data[0], data.size()); }

  // Writes a single byte to the UART bus.
  // @param data The byte to be written.
  void write_byte(uint8_t data) { this->write_array(&data, 1); };

  // Writes a null-terminated string to the UART bus.
  // @param str Pointer to the null-terminated string.
  void write_str(const char *str) {
    const auto *data = reinterpret_cast<const uint8_t *>(str);
    this->write_array(data, strlen(str));
  };

  // Pure virtual method to write an array of bytes to the UART bus.
  // @param data Pointer to the array of bytes.
  // @param len Length of the array.
  virtual void write_array(const uint8_t *data, size_t len) = 0;

  // Reads a single byte from the UART bus.
  // @param data Pointer to the byte where the read data will be stored.
  // @return True if a byte was successfully read, false otherwise.
  bool read_byte(uint8_t *data) { return this->read_array(data, 1); };

  // Pure virtual method to peek the next byte in the UART buffer without removing it.
  // @param data Pointer to the byte where the peeked data will be stored.
  // @return True if a byte is available to peek, false otherwise.
  virtual bool peek_byte(uint8_t *data) = 0;

  // Pure virtual method to read an array of bytes from the UART bus.
  // @param data Pointer to the array where the read data will be stored.
  // @param len Number of bytes to read.
  // @return True if the specified number of bytes were successfully read, false otherwise.
  virtual bool read_array(uint8_t *data, size_t len) = 0;

  // Pure virtual method to return the number of bytes available for reading.
  // @return Number of available bytes.
  virtual int available() = 0;

  // Pure virtual method to block until all bytes have been written to the UART bus.
  virtual void flush() = 0;

  // Sets the TX (transmit) pin for the UART bus.
  // @param tx_pin Pointer to the internal GPIO pin used for transmission.
  void set_tx_pin(InternalGPIOPin *tx_pin) { this->tx_pin_ = tx_pin; }

  // Sets the RX (receive) pin for the UART bus.
  // @param rx_pin Pointer to the internal GPIO pin used for reception.
  void set_rx_pin(InternalGPIOPin *rx_pin) { this->rx_pin_ = rx_pin; }

  // Sets the size of the RX buffer.
  // @param rx_buffer_size Size of the RX buffer in bytes.
  void set_rx_buffer_size(size_t rx_buffer_size) { this->rx_buffer_size_ = rx_buffer_size; }

  // Gets the size of the RX buffer.
  // @return Size of the RX buffer in bytes.
  size_t get_rx_buffer_size() { return this->rx_buffer_size_; }

  // Sets the number of stop bits used in UART communication.
  // @param stop_bits Number of stop bits.
  void set_stop_bits(uint8_t stop_bits) { this->stop_bits_ = stop_bits; }

  // Gets the number of stop bits used in UART communication.
  // @return Number of stop bits.
  uint8_t get_stop_bits() const { return this->stop_bits_; }

  // Set the number of data bits used in UART communication.
  // @param data_bits Number of data bits.
  void set_data_bits(uint8_t data_bits) { this->data_bits_ = data_bits; }

  // Get the number of data bits used in UART communication.
  // @return Number of data bits.
  uint8_t get_data_bits() const { return this->data_bits_; }

  // Set the parity used in UART communication.
  // @param parity Parity option.
  void set_parity(UARTParityOptions parity) { this->parity_ = parity; }

  // Get the parity used in UART communication.
  // @return Parity option.
  UARTParityOptions get_parity() const { return this->parity_; }

  // Set the baud rate for UART communication.
  // @param baud_rate Baud rate in bits per second.
  void set_baud_rate(uint32_t baud_rate) { baud_rate_ = baud_rate; }

  // Get the baud rate for UART communication.
  // @return Baud rate in bits per second.
  uint32_t get_baud_rate() const { return baud_rate_; }

#if defined(USE_ESP8266) || defined(USE_ESP32)
  /**
   * Load the UART settings.
   * @param dump_config If true (default), output the new settings to logs; otherwise, change settings quietly.
   *
   * Example:
   * ```cpp
   * id(uart1).load_settings(false);
   * ```
   *
   * This will load the current UART interface with the latest settings (baud_rate, parity, etc).
   */
  virtual void load_settings(bool dump_config){};

  /**
   * Load the UART settings.
   *
   * Example:
   * ```cpp
   * id(uart1).load_settings();
   * ```
   *
   * This will load the current UART interface with the latest settings (baud_rate, parity, etc).
   */
  virtual void load_settings(){};
#endif  // USE_ESP8266 || USE_ESP32

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
