#pragma once
#include "esphome/core/component.h"

namespace esphome::zephyr_coredump {

/// Print a stored coredump once, then erase it. Also called early from the logger setup.
void print_coredump();

class Coredump : public Component {
 public:
  void setup() override;
  void dump_config() override;
};

}  // namespace esphome::zephyr_coredump
