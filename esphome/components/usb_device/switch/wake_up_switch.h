#pragma once
#ifdef USE_ESP32_VARIANT_ESP32S2
#include "esphome/core/defines.h"
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace usb_device {

class WakeUpSwitch : public switch_::Switch, public Component {
 public:
  void write_state(bool state) override;
  void dump_config() override;
};

}  // namespace usb_device
}  // namespace esphome
#endif
#endif
