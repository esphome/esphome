#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

#ifdef USE_OTA
#include "esphome/components/ota/ota_component.h"
#endif

namespace esphome {
namespace restart {

class RestartSwitch : public switch_::Switch, public Component {
 public:
  void dump_config() override;

  #ifdef USE_OTA
  /// Sets the OTA component to be configured when rebooting
  void set_ota(ota::OTAComponent *ota);

  /// Sets whether the restart switch should enter safe mode on reboot
  void set_safe_mode(bool safe_mode);
  #endif

 protected:
  void write_state(bool state) override;

  #ifdef USE_OTA
  ota::OTAComponent *ota_{nullptr};
  bool safe_mode_{false};
  #endif
};

}  // namespace restart
}  // namespace esphome
