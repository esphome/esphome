#pragma once

#include "esphome/components/uart/uart_component.h"
#include "esphome/core/component.h"

#include <cstddef>

namespace esphome::uart {

/// Copies bytes both ways between two UARTs.
class UARTBridge final : public Component {
 public:
  void set_a(UARTComponent *a) { this->a_ = a; }
  void set_b(UARTComponent *b) { this->b_ = b; }

  void loop() override;
  void dump_config() override;
  // After the UART bus, so both ends are already set up.
  float get_setup_priority() const override { return setup_priority::BUS - 1.0f; }

 protected:
  UARTComponent *a_{nullptr};
  UARTComponent *b_{nullptr};
};

}  // namespace esphome::uart
