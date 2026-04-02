#pragma once

#include "esphome/components/uart/uart.h"

namespace esphome::mitsubishi_cn105 {

class MitsubishiCN105 {
 public:
  explicit MitsubishiCN105(uart::UARTDevice &device) : device_(device) {}

  uint32_t get_update_interval() const { return this->update_interval_ms_; }
  void set_update_interval(uint32_t interval_ms) { this->update_interval_ms_ = interval_ms; }

 protected:
  uart::UARTDevice &device_;
  uint32_t update_interval_ms_{1000};
};

}  // namespace esphome::mitsubishi_cn105
