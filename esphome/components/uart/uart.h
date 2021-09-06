#pragma once

#include <vector>
#include <HardwareSerial.h>
#include "esphome/core/esphal.h"
#include "esphome/core/component.h"

namespace esphome {
namespace uart {

enum UARTParityOptions {
  UART_CONFIG_PARITY_NONE,
  UART_CONFIG_PARITY_EVEN,
  UART_CONFIG_PARITY_ODD,
};

const char *parity_to_str(UARTParityOptions parity);

#ifdef ARDUINO_ARCH_ESP8266
class ESP8266SoftwareSerial {
 public:
  void setup(int8_t tx_pin, int8_t rx_pin, uint32_t baud_rate, uint8_t stop_bits, uint32_t data_bits,
             UARTParityOptions parity, size_t rx_buffer_size);

  uint8_t read_byte();
  uint8_t peek_byte();

  void flush();

  void write_byte(uint8_t data);

  int available();

  GPIOPin *gpio_tx_pin_{nullptr};
  GPIOPin *gpio_rx_pin_{nullptr};

 protected:
  static void gpio_intr(ESP8266SoftwareSerial *arg);

  void wait_(uint32_t *wait, const uint32_t &start);
  bool read_bit_(uint32_t *wait, const uint32_t &start);
  void write_bit_(bool bit, uint32_t *wait, const uint32_t &start);

  uint32_t bit_time_{0};
  uint8_t *rx_buffer_{nullptr};
  size_t rx_buffer_size_;
  volatile size_t rx_in_pos_{0};
  size_t rx_out_pos_{0};
  uint8_t stop_bits_;
  uint8_t data_bits_;
  UARTParityOptions parity_;
  ISRInternalGPIOPin *tx_pin_{nullptr};
  ISRInternalGPIOPin *rx_pin_{nullptr};
};
#endif

class UARTComponent : public Component, public Stream {
 public:
  void set_baud_rate(uint32_t baud_rate) { baud_rate_ = baud_rate; }
  uint32_t get_baud_rate() const { return baud_rate_; }

  uint32_t get_config();

  void setup() override;

  void dump_config() override;

  void write_byte(uint8_t data);

  void write_array(const uint8_t *data, size_t len);
  void write_array(const std::vector<uint8_t> &data) { this->write_array(&data[0], data.size()); }

  void write_str(const char *str);

  bool peek_byte(uint8_t *data);

  bool read_byte(uint8_t *data);

  bool read_array(uint8_t *data, size_t len);

  int available() override;

  /// Block until all bytes have been written to the UART bus.
  void flush() override;

  float get_setup_priority() const override { return setup_priority::BUS; }

  size_t write(uint8_t data) override;
  int read() override;
  int peek() override;

  void set_tx_pin(uint8_t tx_pin) { this->tx_pin_ = tx_pin; }
  void set_rx_pin(uint8_t rx_pin) { this->rx_pin_ = rx_pin; }
  void set_rx_buffer_size(size_t rx_buffer_size) { this->rx_buffer_size_ = rx_buffer_size; }
#ifdef ARDUINO_ARCH_ESP32
  void set_invert(bool invert) { this->invert_ = invert; }
#endif
  void set_stop_bits(uint8_t stop_bits) { this->stop_bits_ = stop_bits; }
  void set_data_bits(uint8_t data_bits) { this->data_bits_ = data_bits; }
  void set_parity(UARTParityOptions parity) { this->parity_ = parity; }

 protected:
  void check_logger_conflict_();
  bool check_read_timeout_(size_t len = 1);
  friend class UARTDevice;

  HardwareSerial *hw_serial_{nullptr};
#ifdef ARDUINO_ARCH_ESP8266
  ESP8266SoftwareSerial *sw_serial_{nullptr};
#endif
  optional<uint8_t> tx_pin_;
  optional<uint8_t> rx_pin_;
  size_t rx_buffer_size_;
#ifdef ARDUINO_ARCH_ESP32
  bool invert_;
#endif
  uint32_t baud_rate_;
  uint8_t stop_bits_;
  uint8_t data_bits_;
  UARTParityOptions parity_;

 private:
#ifdef ARDUINO_ARCH_ESP8266
  static bool serial0InUse;
#endif
};

#ifdef ARDUINO_ARCH_ESP32
extern uint8_t next_uart_num;
#endif

class UARTDevice : public Stream {
 public:
  UARTDevice() = default;
  UARTDevice(UARTComponent *parent) : parent_(parent) {}

  void set_uart_parent(UARTComponent *parent) { this->parent_ = parent; }

  void write_byte(uint8_t data) { this->parent_->write_byte(data); }

  void write_array(const uint8_t *data, size_t len) { this->parent_->write_array(data, len); }
  void write_array(const std::vector<uint8_t> &data) { this->parent_->write_array(data); }
  template<size_t N> void write_array(const std::array<uint8_t, N> &data) {
    this->parent_->write_array(data.data(), data.size());
  }

  void write_str(const char *str) { this->parent_->write_str(str); }

  bool read_byte(uint8_t *data) { return this->parent_->read_byte(data); }
  bool peek_byte(uint8_t *data) { return this->parent_->peek_byte(data); }

  bool read_array(uint8_t *data, size_t len) { return this->parent_->read_array(data, len); }
  template<size_t N> optional<std::array<uint8_t, N>> read_array() {  // NOLINT
    std::array<uint8_t, N> res;
    if (!this->read_array(res.data(), N)) {
      return {};
    }
    return res;
  }

  int available() override { return this->parent_->available(); }

  void flush() override { return this->parent_->flush(); }

  size_t write(uint8_t data) override { return this->parent_->write(data); }
  int read() override { return this->parent_->read(); }
  int peek() override { return this->parent_->peek(); }

  /// Check that the configuration of the UART bus matches the provided values and otherwise print a warning
  void check_uart_settings(uint32_t baud_rate, uint8_t stop_bits = 1,
                           UARTParityOptions parity = UART_CONFIG_PARITY_NONE, uint8_t data_bits = 8);

 protected:
  UARTComponent *parent_{nullptr};
};

}  // namespace uart
}  // namespace esphome
