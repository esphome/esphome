#pragma once
#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "esphome/components/uart/uart_component_esp_idf.h"
#include "esphome/components/usb_cdc_acm/usb_cdc_acm.h"
#include "esphome/core/component.h"

#include "freertos/ringbuf.h"
#include "tusb_cdc_acm.h"

namespace esphome {
namespace usb_uart_bridge {

class USBUARTBridge : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_dtr_pin(GPIOPin *dtr_pin) { this->dtr_pin_ = dtr_pin; }
  void set_rts_pin(GPIOPin *rts_pin) { this->rts_pin_ = rts_pin; }

  void set_uart_parent(uart::IDFUARTComponent *uart_parent) { this->uart_parent_ = uart_parent; }
  void set_usb_cdc_parent(usb_cdc_acm::USBCDCACMInstance *usb_cdc_parent) { this->usb_cdc_parent_ = usb_cdc_parent; }

  uart::IDFUARTComponent *get_uart_parent() const { return this->uart_parent_; }
  usb_cdc_acm::USBCDCACMInstance *get_usb_cdc_parent() const { return this->usb_cdc_parent_; }

  void request_uart_settings_reload() { this->reload_uart_settings_ = millis(); }

  void set_line_coding(uint32_t bit_rate, uint8_t stop_bits, uint8_t parity, uint8_t data_bits);
  void set_line_state(bool dtr, bool rts);

  static void uart_rx_task_fn(void *arg);
  static void uart_tx_task_fn(void *arg);
  void uart_rx_task();
  void uart_tx_task();

  typedef struct uart_rx_task_param_t {
    TaskHandle_t usb_tx_handle;
    QueueHandle_t uart_queue;
  } uart_rx_task_param_t;

 protected:
  TaskHandle_t uart_rx_task_handle_{nullptr};
  TaskHandle_t uart_tx_task_handle_{nullptr};

  GPIOPin *dtr_pin_{nullptr};
  GPIOPin *rts_pin_{nullptr};

  uint32_t reload_uart_settings_{0};

  uart_rx_task_param_t uart_rx_task_param_{};

  uart::IDFUARTComponent *uart_parent_{nullptr};
  usb_cdc_acm::USBCDCACMInstance *usb_cdc_parent_{nullptr};
};

}  // namespace usb_uart_bridge
}  // namespace esphome
#endif
