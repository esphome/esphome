#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

#include <vector>

namespace esphome {
namespace custom {

class CustomSwitchConstructor : public Component {
 public:
  CustomSwitchConstructor(const std::function<std::vector<switch_::Switch *>()> &init) { this->switches_ = init(); }

  switch_::Switch *get_switch(int i) { return this->switches_[i]; }

  void dump_config() override;

 protected:
  std::vector<switch_::Switch *> switches_;
};

}  // namespace custom
}  // namespace esphome
