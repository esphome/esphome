#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace copy {

class CopySwitch : public switch_::Switch, public Component {
 public:
  void set_source(switch_::Switch *source) { source_ = source; }
  void setup() override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;

  switch_::Switch *source_;
};

}  // namespace copy
}  // namespace esphome
