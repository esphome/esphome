#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/hbridge/hbridge.h"

namespace esphome {
namespace hbridge {

enum ValveActuatorRestoreMode {
  HBRIDGE_SWITCH_RESTORE_DEFAULT_OFF,
  HBRIDGE_SWITCH_RESTORE_DEFAULT_ON,
  HBRIDGE_SWITCH_ALWAYS_OFF,
  HBRIDGE_SWITCH_ALWAYS_ON,
  HBRIDGE_SWITCH_RESTORE_INVERTED_DEFAULT_OFF,
  HBRIDGE_SWITCH_RESTORE_INVERTED_DEFAULT_ON,
};

class HBridgeSwitch : public HBridge, public switch_::Switch {
 public:
  // Config set functions
  void set_switching_duration(uint32_t switching_duration) { this->switching_signal_duration_ = switching_duration; }
  void set_restore_mode(ValveActuatorRestoreMode restore_mode) { this->restore_mode_ = restore_mode; }

  // Component interfacing/overriding from HBridge base class
  void setup() override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;

  ValveActuatorRestoreMode restore_mode_{HBRIDGE_SWITCH_RESTORE_DEFAULT_OFF};
  uint32_t switching_signal_duration_{0};  // Default 0 milliseconds = forever
};

}  // namespace hbridge
}  // namespace esphome
