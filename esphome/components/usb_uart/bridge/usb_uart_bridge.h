#pragma once
#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "esphome/components/uart/uart_component_esp_idf.h"
#include "esphome/components/usb_cdc_acm/usb_cdc_acm.h"
#include "esphome/core/component.h"

#include <memory>
#include "freertos/ringbuf.h"
#include "tinyusb_cdc_acm.h"

namespace esphome::usb_uart_bridge {

class USBUARTBridge : public Component {
 public:
  USBUARTBridge(uart::IDFUARTComponent *uart_parent, usb_cdc_acm::USBCDCACMInstance *usb_cdc_parent,
                size_t uart_rx_buffer_size, size_t uart_tx_buffer_size)
      : uart_rx_buffer_size_(uart_rx_buffer_size),
        uart_tx_buffer_size_(uart_tx_buffer_size),
        uart_parent_(uart_parent),
        usb_cdc_parent_(usb_cdc_parent) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_dtr_pin(GPIOPin *dtr_pin) { this->dtr_pin_ = dtr_pin; }
  void set_rts_pin(GPIOPin *rts_pin) { this->rts_pin_ = rts_pin; }

  void set_line_coding();
  void set_line_state(bool dtr, bool rts);

 protected:
  static void uart_rx_task_fn(void *arg);
  static void uart_tx_task_fn(void *arg);
  void uart_rx_task_();
  void uart_tx_task_();
  void uart_settings_reload_();

  TaskHandle_t uart_rx_task_handle_{nullptr};
  TaskHandle_t uart_tx_task_handle_{nullptr};
  TaskHandle_t usb_tx_task_handle_{nullptr};

  GPIOPin *dtr_pin_{nullptr};
  GPIOPin *rts_pin_{nullptr};

  uint32_t reload_requested_at_{0};

  size_t uart_rx_buffer_size_;
  size_t uart_tx_buffer_size_;
  std::unique_ptr<uint8_t[]> uart_rx_buffer_{nullptr};
  std::unique_ptr<uint8_t[]> uart_tx_buffer_{nullptr};

  uart::IDFUARTComponent *uart_parent_;
  usb_cdc_acm::USBCDCACMInstance *usb_cdc_parent_;

  bool reload_pending_{false};
};

}  // namespace esphome::usb_uart_bridge
#endif
