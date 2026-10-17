#pragma once
#include "esphome/core/component.h"

namespace esphome::zephyr_coredump {

/// Tell the component a log reader is present. The logger calls this at boot on a UART and each time
/// the port is opened on USB CDC; the stored dump is then printed from the start and erased.
void print_coredump();
/// Tell the component the reader went away (the USB port was closed); the dump is kept for the next one.
void stop_coredump();

class Coredump : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  friend void print_coredump();
  friend void stop_coredump();
  /// Start printing after a short delay, one line per loop so the USB CDC transmit buffer and the main loop keep up
  void start_print_();
  void stop_print_();
  void finish_(int err);

  uint32_t offset_{0};
  uint32_t total_{0};
  int verify_{0};  // result of verifying the stored dump, 1 when it is good
  bool stored_{false};
};

}  // namespace esphome::zephyr_coredump
