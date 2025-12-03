#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace wts01 {

constexpr uint8_t PACKET_SIZE = 9;

class WTS01Sensor : public sensor::Sensor, public uart::UARTDevice, public Component {
 public:
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  uint8_t buffer_[PACKET_SIZE];
  uint8_t buffer_pos_{0};

  void handle_char_(uint8_t c);
  void process_packet_();
};

}  // namespace wts01
}  // namespace esphome
