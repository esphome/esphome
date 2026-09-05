#pragma once

#ifdef USE_ESP32

#include <driver/uart.h>
#include "esphome/core/component.h"
#include "uart_component.h"
#ifdef USE_UART_WAKE_LOOP_ON_RX
#include <driver/uart_select.h>
#endif

namespace esphome::uart {

/// ESP-IDF UART driver wrapper.
///
/// Thread safety: All public methods must only be called from the main loop.
/// The ESP-IDF UART driver API does not guarantee thread safety, and ESPHome's
/// peek byte state (has_peek_/peek_byte_) is not synchronized.
class IDFUARTComponent final : public UARTComponent, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void set_rx_full_threshold(size_t rx_full_threshold) override;
  void set_rx_timeout(size_t rx_timeout) override;

  void write_array(const uint8_t *data, size_t len) override;

  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;

  size_t available() override;
  UARTFlushResult flush() override;

  void set_flush_timeout(uint32_t flush_timeout_ms) override { this->flush_timeout_ms_ = flush_timeout_ms; }

  uint8_t get_hw_serial_number() { return this->uart_num_; }

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
  using UARTComponent::load_settings;  // also bring in the no-arg overload for convenience

  /**
   * Apply the current framing settings (baud rate, parity, data/stop bits) to the
   * already-installed driver without the driver delete/reinstall that load_settings()
   * performs. The driver ring buffers and any tasks blocked in
   * uart_read_bytes()/uart_write_bytes() survive, but uart_param_config() flushes
   * both hardware FIFOs, so up to a FIFO's worth of in-flight bytes is discarded in
   * each direction. Falls back to a full reload if the driver is not installed.
   */
  void apply_settings_live();

  void on_shutdown() override;

 protected:
  void check_logger_conflict() override;
  // Signal-inversion flags derived from the configured pins' inverted markers.
  uint32_t line_inversion_mask_();
  // (Re)apply the settings that uart_param_config() resets: line inversion, RX full
  // threshold, RX timeout, and port mode. Returns the first error, already logged.
  esp_err_t apply_line_settings_();
  uart_port_t uart_num_{UART_NUM_MAX};
  // Fallback for a failed live reconfiguration (unachievable requested rate).
  uint32_t last_good_baud_{0};
  uart_config_t get_config_();

  bool has_peek_{false};
  uint8_t peek_byte_;
  uint32_t flush_timeout_ms_{0};  ///< 0 means wait indefinitely (portMAX_DELAY).

#ifdef USE_UART_WAKE_LOOP_ON_RX
  // ISR callback for UART RX data notification — wakes the main loop directly.
  static void uart_rx_isr_callback(uart_port_t uart_num, uart_select_notif_t uart_select_notif, BaseType_t *task_woken);
#endif  // USE_UART_WAKE_LOOP_ON_RX
};

}  // namespace esphome::uart
#endif  // USE_ESP32
