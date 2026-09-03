#pragma once

#include "esphome/core/component.h"
#include <atomic>

namespace esphome::wake_test_component {

class WakeTestComponent : public Component {
 public:
  void setup() override {}
  void loop() override { this->loop_count_.fetch_add(1, std::memory_order_relaxed); }

  int get_loop_count() const { return this->loop_count_.load(std::memory_order_relaxed); }

  // Spawn a detached thread that sleeps briefly then calls
  // App.wake_loop_threadsafe(). Used by the integration test to verify a
  // cross-thread wake forces a component-phase iteration even when
  // loop_interval_ has been raised high enough to gate it off otherwise.
  void start_async_wake();

  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  std::atomic<int> loop_count_{0};
};

}  // namespace esphome::wake_test_component
