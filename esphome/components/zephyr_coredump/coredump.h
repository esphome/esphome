#pragma once
#include "esphome/core/component.h"

namespace esphome::zephyr_coredump {

/// Tell the component a log reader is present. The logger calls this at boot on a UART and each time
/// the port is opened on USB CDC; the stored dump is then printed and erased.
void print_coredump();

class Coredump : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  friend void print_coredump();
  /// Start printing after a short delay, one line per loop so the USB CDC transmit buffer and the main loop keep up
  void start_print_();
  void finish_(int err);

  uint32_t offset_{0};
  uint32_t total_{0};
};

}  // namespace esphome::zephyr_coredump
