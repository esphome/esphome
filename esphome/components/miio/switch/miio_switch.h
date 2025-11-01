#pragma once

#include "esphome/core/component.h"
#include "esphome/components/miio/miio.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace miio {

class MiioSwitch : public switch_::Switch, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void set_switch_id(uint8_t cluster, uint8_t key) { this->switch_id_ = std::make_tuple(cluster, key); }
  void set_miio_parent(Miio *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override;

  Miio *parent_;
  prop_t switch_id_;
};

}  // namespace miio
}  // namespace esphome
