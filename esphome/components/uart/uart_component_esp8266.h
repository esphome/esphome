#pragma once

#ifdef USE_ESP8266

#include <HardwareSerial.h>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "software_serial_rx_decoder.h"
#include "uart_component.h"

namespace esphome::uart {

class ESP8266SoftwareSerial {
 public:
  void setup(InternalGPIOPin *tx_pin, InternalGPIOPin *rx_pin, uint32_t baud_rate, uint8_t stop_bits,
             uint32_t data_bits, UARTParityOptions parity, size_t rx_buffer_size);

  uint8_t read_byte();
  uint8_t peek_byte();

  void flush();

  void write_byte(uint8_t data);

  size_t available();

#ifdef USE_UART_AUTO_BAUD
  /// Call after setup(); edge mode only.
  void set_auto_baud_sync(uint8_t sync) { this->rx_.set_auto_baud_sync(sync); }
#endif

  /// TX holds interrupts off for whole frames: above the edge mode limit, or while any instance's RX interrupt reads
  /// whole bytes (it must not run between TX bits).
  bool locks_whole_frame() const { return this->lock_frame_ || blocking_rx_count != 0; }

 protected:
  /// Above SW_SERIAL_EDGE_MODE_MAX_BAUD; reads the whole byte in the ISR.
  static void gpio_intr(ESP8266SoftwareSerial *arg);
  static void gpio_intr_edge(ESP8266SoftwareSerial *arg);

  void wait_(uint32_t *wait, const uint32_t &start, uint32_t bit_time);
  bool read_bit_(uint32_t *wait, const uint32_t &start, uint32_t bit_time);
  void write_bit_(bool bit, uint32_t *wait, const uint32_t &start, uint32_t bit_time);

  void rx_finalize_pending_();
  void ESPHOME_ALWAYS_INLINE rx_sync_() {
    if (this->rx_.pending())
      this->rx_finalize_pending_();
  }

  // Members ordered largest to smallest to minimize padding
  uint32_t bit_time_{0};
  InternalGPIOPin *gpio_tx_pin_{nullptr};
  ISRInternalGPIOPin tx_pin_;
  InternalGPIOPin *gpio_rx_pin_{nullptr};
  ISRInternalGPIOPin rx_pin_;
  SoftwareSerialRxDecoder rx_;
  UARTParityOptions parity_;
  uint8_t stop_bits_;
  uint8_t data_bits_;
  bool lock_frame_{false};
  static inline uint8_t blocking_rx_count = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
};

class ESP8266UartComponent final : public UARTComponent, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void write_array(const uint8_t *data, size_t len) override;

  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;

  size_t available() override;
  size_t available_for_write() override;
  UARTFlushResult flush() override;

  uint32_t get_config();

  /**
   * Load the UART with the current settings.
   * @param dump_config (Optional, default `true`): True for displaying new settings or
   * false to change it quietly
   *
   * Example:
   * ```cpp
   * id(uart1).load_settings();
   * ```
   *
   * This will load the current UART interface with the latest settings (baud_rate, parity, etc).
   */
  void load_settings(bool dump_config) override;
  using UARTComponent::load_settings;  // also bring in the no-arg overload for convenience

#ifdef USE_UART_AUTO_BAUD
  /// Software serial only: re-time the receiver on `sync`, the first byte after an idle line.
  void set_auto_baud_sync(uint8_t sync) { this->auto_baud_sync_ = sync; }
#endif

 protected:
  void check_logger_conflict() override;
  void setup_software_serial_();

  HardwareSerial *hw_serial_{nullptr};
  ESP8266SoftwareSerial *sw_serial_{nullptr};
#ifdef USE_UART_AUTO_BAUD
  int16_t auto_baud_sync_{-1};  // -1: off
#endif

 private:
  static bool serial0_in_use;   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
  static bool serial0_swapped;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
};

}  // namespace esphome::uart
#endif  // USE_ESP8266
