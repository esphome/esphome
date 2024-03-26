#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "../bh1745.h"

namespace esphome {
namespace bh1745 {

class BH1745SwitchLed : public switch_::Switch, public Component {
 public:
  void dump_config() override;

  void set_bh1745(BH1745Component *bh1745) { this->bh1745_ = bh1745; }

 protected:
  void write_state(bool state) override;
  BH1745Component *bh1745_;
};
}  // namespace bh1745
}  // namespace esphome
