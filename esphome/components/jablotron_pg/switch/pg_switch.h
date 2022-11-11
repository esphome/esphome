#pragma once
#include "esphome/components/jablotron/jablotron_device.h"
#include "esphome/components/switch/switch.h"

namespace esphome::jablotron_pg {

class PGSwitch : public jablotron::PGDevice, public switch_::Switch {
 public:
  void write_state(bool state) override;
  void register_parent(jablotron::JablotronComponent &parent) override;
  void set_state(bool state) override;
};

}  // namespace esphome::jablotron_pg
