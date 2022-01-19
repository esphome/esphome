#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ota/ota_component.h"
#include "esphome/components/button/button.h"

namespace esphome {
namespace safe_mode {

class SafeModeButton : public button::Button, public Component {
 public:
  void dump_config() override;
  void set_ota(ota::OTAComponent *ota);

 protected:
  ota::OTAComponent *ota_;
  void press_action() override;
};

}  // namespace safe_mode
}  // namespace esphome
