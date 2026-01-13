#pragma once

#ifdef USE_ESP32

#include <driver/uart.h>
#include "esphome/core/component.h"
#include "uart_component.h"

namespace esphome::uart {

class IDFUARTComponent : public UARTComponent, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void set_rx_full_threshold(size_t rx_full_threshold) override;
  void set_rx_timeout(size_t rx_timeout) override;

  void write_array(const uint8_t *data, size_t len) override;

  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;

  int available() override;
  void flush() override;

  uint8_t get_hw_serial_number() { return this->uart_num_; }
  QueueHandle_t *get_uart_event_queue() { return &this->uart_event_queue_; }

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
  uart_port_t uart_num_;
  QueueHandle_t uart_event_queue_;
  uart_config_t get_config_();
  SemaphoreHandle_t lock_;

  bool has_peek_{false};
  uint8_t peek_byte_;

#ifdef USE_UART_WAKE_LOOP_ON_RX
  // RX notification support
  void start_rx_event_task_();
  static void rx_event_task_func(void *param);

  TaskHandle_t rx_event_task_handle_{nullptr};
#endif  // USE_UART_WAKE_LOOP_ON_RX
};

}  // namespace esphome::uart
#endif  // USE_ESP32
