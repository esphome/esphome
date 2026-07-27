#pragma once
#ifdef USE_ZEPHYR

#include "esphome/core/component.h"
#include "uart_component.h"
#include <atomic>
#include <memory>
#include <zephyr/device.h>
#include <zephyr/sys/ring_buffer.h>

namespace esphome::uart {

enum ZephyrUartPort : uint8_t {
  ZEPHYR_UART_PORT_0 = 0,
  ZEPHYR_UART_PORT_1 = 1,
};

class ZephyrUartComponent : public UARTComponent, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }
  void write_array(const uint8_t *data, size_t len) override;
  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;
  size_t available() override;
  UARTFlushResult flush() override;
  void set_port(ZephyrUartPort port) { this->port_ = port; }

 protected:
  void check_logger_conflict() override {}

  static void uart_irq_handler_s(const struct device *dev, void *user_data);
  void uart_irq_handler_();

  const struct device *uart_dev_{nullptr};
  ZephyrUartPort port_{ZEPHYR_UART_PORT_0};

  // Interrupt-driven RX ring buffer — allocated once in setup() from rx_buffer_size_
  std::unique_ptr<uint8_t[]> rx_buf_mem_;
  struct ring_buf rx_rb_;
  // Written from uart_irq_handler_() (ISR context), read from dump_config() (main
  // thread) -- atomic to avoid a torn read-modify-write across that boundary.
  std::atomic<uint32_t> rx_overflow_count_{0};
};

}  // namespace esphome::uart
#endif  // USE_ZEPHYR
