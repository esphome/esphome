#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/ota/ota_component.h"

namespace esphome {
namespace restart {

class RestartSwitch : public switch_::Switch, public Component {
 public:
  void dump_config() override;

  /// Sets the OTA component to be configured when rebooting
  void set_ota(ota::OTAComponent *ota);

  /// Sets whether the restart switch should enter safe mode on reboot
  void set_safe_mode(bool safe_mode);

 protected:
  void write_state(bool state) override;
  ota::OTAComponent *ota_{nullptr};
  bool safe_mode_{false};
};

}  // namespace restart
}  // namespace esphome
