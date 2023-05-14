#pragma once

#ifdef USE_ESP_IDF

#include <driver/uart.h>
#include "esphome/core/component.h"
#include "uart_component.h"

namespace esphome {
namespace uart {

class IDFUARTComponent : public UARTComponent, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void write_array(const uint8_t *data, size_t len) override;

  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;

  int available() override;
  void flush() override;

  uint8_t get_hw_serial_number() { return this->uart_num_; }
  QueueHandle_t *get_uart_event_queue() { return &this->uart_event_queue_; }

 protected:
  void check_logger_conflict() override;
  uart_port_t uart_num_;
  QueueHandle_t uart_event_queue_;
  uart_config_t get_config_();
  SemaphoreHandle_t lock_;

  bool has_peek_{false};
  uint8_t peek_byte_;
};

}  // namespace uart
}  // namespace esphome

#endif  // USE_ESP_IDF
