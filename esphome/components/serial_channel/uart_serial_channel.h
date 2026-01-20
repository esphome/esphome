#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/serial_channel/serial_channel.h"
#include <vector>

namespace esphome::serial_channel {
class UARTSerialChannel : public SerialChannel, public uart::UARTDevice, public Component {
 public:
  UARTSerialChannel(size_t buffer_size) { this->rx_buffer_.init(buffer_size); }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(const uint8_t *data, size_t len) override;

  FixedVector<uint8_t> rx_buffer_;
};
}  // namespace esphome::serial_channel
