#pragma once
#include "esphome/core/component.h"
namespace esphome::tcpbench {
class TcpBench : public Component {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
};
}  // namespace esphome::tcpbench
