#pragma once

#include "esphome/components/esphome/ota/ota_esphome.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

namespace esphome {
namespace safe_mode {

class SafeModeSwitch : public switch_::Switch, public Component {
 public:
  void dump_config() override;
  void set_ota(ota_esphome::OTAESPHomeComponent *ota);

 protected:
  ota_esphome::OTAESPHomeComponent *ota_;
  void write_state(bool state) override;
};

}  // namespace safe_mode
}  // namespace esphome
