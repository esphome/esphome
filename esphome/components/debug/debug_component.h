#pragma once

#include "esphome/core/component.h"

namespace esphome {
namespace debug {

class DebugComponent : public Component {
 public:
  void loop() override;
  float get_setup_priority() const override;
  void dump_config() override;

 protected:
  uint32_t free_heap_{};
};

}  // namespace debug
}  // namespace esphome
