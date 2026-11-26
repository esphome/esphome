#pragma once
#include "esphome/core/component.h"
namespace esphome::tcpbench {
class TcpBench : public Component {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  uint32_t prx_{0}, ptx_{0};
};
}  // namespace esphome::tcpbench
