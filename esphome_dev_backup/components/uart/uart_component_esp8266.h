#pragma once

#ifdef USE_ESP8266

#include <HardwareSerial.h>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "uart_component.h"

namespace esphome {
namespace uart {

class ESP8266SoftwareSerial {
 public:
  void setup(InternalGPIOPin *tx_pin, InternalGPIOPin *rx_pin, uint32_t baud_rate, uint8_t stop_bits,
             uint32_t data_bits, UARTParityOptions parity, size_t rx_buffer_size);

  uint8_t read_byte();
  uint8_t peek_byte();

  void flush();

  void write_byte(uint8_t data);

  int available();

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
  InternalGPIOPin *gpio_tx_pin_{nullptr};
  ISRInternalGPIOPin tx_pin_;
  InternalGPIOPin *gpio_rx_pin_{nullptr};
  ISRInternalGPIOPin rx_pin_;
};

class ESP8266UartComponent : public UARTComponent, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void write_array(const uint8_t *data, size_t len) override;

  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;

  int available() override;
  void flush() override;

  uint32_t get_config();

  /**
   * Load the UART with the current settings.
   * @param dump_config (Optional, default `true`): True for displaying new settings or
   * false to change it quitely
   *
   * Example:
   * ```cpp
   * id(uart1).load_settings();
   * ```
   *
   * This will load the current UART interface with the latest settings (baud_rate, parity, etc).
   */
  void load_settings(bool dump_config) override;
  void load_settings() override { this->load_settings(true); }

 protected:
  void check_logger_conflict() override;

  HardwareSerial *hw_serial_{nullptr};
  ESP8266SoftwareSerial *sw_serial_{nullptr};

 private:
  static bool serial0_in_use;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
};

}  // namespace uart
}  // namespace esphome

#endif  // USE_ESP8266
