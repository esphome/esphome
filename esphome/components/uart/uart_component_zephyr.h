#pragma once

#ifdef USE_ZEPHYR

#include <zephyr/device.h>
#include <zephyr/sys/ring_buffer.h>

#include "esphome/core/component.h"
#include "uart_component.h"

namespace esphome::uart {

class ZephyrUARTComponent final : public UARTComponent, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void write_array(const uint8_t *data, size_t len) override;

  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;

  size_t available() override;
  UARTFlushResult flush() override;

  void set_device(const struct device *device) { this->device_ = device; }

  void load_settings(bool dump_config) override;
  using UARTComponent::load_settings;

 protected:
  void check_logger_conflict() override {}
  bool configure_();
  static void rx_isr(const struct device *device, void *user_data);

  const struct device *device_{nullptr};
  struct ring_buf rx_ring_{};
  uint8_t *rx_storage_{nullptr};
  // Bytes dropped because the RX buffer was full; written in the ISR, reported from loop()
  volatile uint32_t rx_overflow_{0};
  uint32_t rx_overflow_reported_{0};
  // When the last byte written started; flush() waits for it to finish
  uint32_t last_tx_start_us_{0};
  uint8_t peek_buffer_{0};
  bool has_peek_{false};
  bool tx_pending_{false};
};

}  // namespace esphome::uart

#endif  // USE_ZEPHYR
