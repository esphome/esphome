#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ota/ota_component.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace safe_mode {

class SafeModeSwitch : public switch_::Switch, public Component {
 public:
  void dump_config() override;
  void set_ota(ota::OTAComponent *ota);

 protected:
  ota::OTAComponent *ota_;
  void write_state(bool state) override;
};

}  // namespace safe_mode
}  // namespace esphome
