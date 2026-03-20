#pragma once

#include "esphome/core/component.h"
#include <atomic>

namespace esphome {
namespace defer_stress_component {

class DeferStressComponent : public Component {
 public:
  void setup() override;
  void run_multi_thread_test();

 private:
  std::atomic<int> total_defers_{0};
  std::atomic<int> executed_defers_{0};
};

}  // namespace defer_stress_component
}  // namespace esphome
