#include "hbridge_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hbridge {

static const char *const TAG = "switch.hbridge";


void HBridgeSwitch::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Valve Actuator: '%s'...", this->name_.c_str());

  bool initial_state = false;
  switch (this->restore_mode_) {
    case HBRIDGE_SWITCH_RESTORE_DEFAULT_OFF:
      initial_state = this->get_initial_state().value_or(false);
      break;
    case HBRIDGE_SWITCH_RESTORE_DEFAULT_ON:
      initial_state = this->get_initial_state().value_or(true);
      break;
    case HBRIDGE_SWITCH_RESTORE_INVERTED_DEFAULT_OFF:
      initial_state = !this->get_initial_state().value_or(true);
      break;
    case HBRIDGE_SWITCH_RESTORE_INVERTED_DEFAULT_ON:
      initial_state = !this->get_initial_state().value_or(false);
      break;
    case HBRIDGE_SWITCH_ALWAYS_OFF:
      initial_state = false;
      break;
    case HBRIDGE_SWITCH_ALWAYS_ON:
      initial_state = true;
      break;
  }

  // write state before setup
  if (initial_state)
    this->turn_on();
  else
    this->turn_off();

  //Setup output pins
  HBridge::setup();

  // write after setup again for other IOs
  if (initial_state)
    this->turn_on();
  else
    this->turn_off();
}

void HBridgeSwitch::loop() { 
  HBridge::loop();
}


void HBridgeSwitch::dump_config() {
  LOG_SWITCH("", "Switch", this);
  ESP_LOGCONFIG(TAG, "  Switching signal duration: %d", this->switching_signal_duration_);

  const LogString *restore_mode = LOG_STR("");
  switch (this->restore_mode_) {
    case HBRIDGE_SWITCH_RESTORE_DEFAULT_OFF:
      restore_mode = LOG_STR("Restore (Defaults to OFF)");
      break;
    case HBRIDGE_SWITCH_RESTORE_DEFAULT_ON:
      restore_mode = LOG_STR("Restore (Defaults to ON)");
      break;
    case HBRIDGE_SWITCH_RESTORE_INVERTED_DEFAULT_ON:
      restore_mode = LOG_STR("Restore inverted (Defaults to ON)");
      break;
    case HBRIDGE_SWITCH_RESTORE_INVERTED_DEFAULT_OFF:
      restore_mode = LOG_STR("Restore inverted (Defaults to OFF)");
      break;
    case HBRIDGE_SWITCH_ALWAYS_OFF:
      restore_mode = LOG_STR("Always OFF");
      break;
    case HBRIDGE_SWITCH_ALWAYS_ON:
      restore_mode = LOG_STR("Always ON");
      break;
  }
  ESP_LOGCONFIG(TAG, "  Restore Mode: %s", LOG_STR_ARG(restore_mode));
}


void HBridgeSwitch::write_state(bool state) 
{
  ESP_LOGCONFIG(TAG, "Set state: %d", state);

  // cancel any pending "state changes"
  this->cancel_timeout("switch_signal_timeout");

  //Set HBridge output states to desired state (direction is relative, can be inverted by config or wiring)
  if(state){
    HBridge::set_state(HBRIDGE_MODE_DIRECTION_A, 1);
  }
  else{
    HBridge::set_state(HBRIDGE_MODE_DIRECTION_B, 1);
  }

  //If we have a switching signal duration, set timeout to disable the signal after the duration
  if(this->switching_signal_duration_ > 0){
    this->set_timeout("switch_signal_timeout", this->switching_signal_duration_, [this, state] 
    {
      //Put actuator motor back to idle
      HBridge::set_state(HBRIDGE_MODE_OFF, 1);

      ESP_LOGCONFIG(TAG, "Switching signal end (%dms)", this->switching_signal_duration_);
    });
  }

  //Publish new state
  this->publish_state(state);
}

}  // namespace hbridge
}  // namespace esphome
