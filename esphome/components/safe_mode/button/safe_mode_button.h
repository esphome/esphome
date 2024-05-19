#pragma once

#include "esphome/components/button/button.h"
#include "esphome/components/esphome/ota/ota_esphome.h"
#include "esphome/core/component.h"

namespace esphome {
namespace safe_mode {

class SafeModeButton : public button::Button, public Component {
 public:
  void dump_config() override;
  void set_ota(esphome::ESPHomeOTAComponent *ota);

 protected:
  esphome::ESPHomeOTAComponent *ota_;
  void press_action() override;
};

}  // namespace safe_mode
}  // namespace esphome
