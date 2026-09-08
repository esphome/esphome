#pragma once

#include "esphome/core/component.h"

namespace esphome::sndbuf_pin {

// Test-only (host): pins SO_SNDBUF on every open TCP socket so integration
// tests get deterministic backpressure; an explicit SO_SNDBUF also disables
// kernel autotuning, and accepted sockets inherit it from the listener.
class SndbufPinComponent : public Component {
 public:
  explicit SndbufPinComponent(int buffer_size) : buffer_size_(buffer_size) {}
  void setup() override;
  // After the api server so its listening socket exists
  float get_setup_priority() const override { return setup_priority::LATE; }

 protected:
  int buffer_size_;
};

}  // namespace esphome::sndbuf_pin
