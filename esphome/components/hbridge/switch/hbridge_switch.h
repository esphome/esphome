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

class HBridgeSwitch : public switch_::Switch, public Component, public hbridge::HBridge {
 public:

  //Config set functions
  void set_switching_duration(uint32_t switching_duration) { this->switching_signal_duration_ = switching_duration; }
  void set_restore_mode(ValveActuatorRestoreMode restore_mode) { this->restore_mode_ = restore_mode; }

  //Component interfacing
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(bool state) override;

  ValveActuatorRestoreMode restore_mode_{HBRIDGE_SWITCH_RESTORE_DEFAULT_OFF}; 
  uint32_t switching_signal_duration_{0}; //Default 0 milliseconds = forever
};

}  // namespace valve_actuator
}  // namespace esphome
