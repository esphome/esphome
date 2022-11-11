#pragma once
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace jablotron {

class UARTLineDevice : public uart::UARTDevice {
 public:
  bool line_buffer_empty() const;
  void set_flow_control_pin(GPIOPin *flow_control_pin);
  void setup();
  std::vector<std::string> read_lines();
  void write_line(std::string str);

 private:
  std::string line_;
  GPIOPin *flow_control_pin_ = nullptr;
};

}  // namespace jablotron
}  // namespace esphome
