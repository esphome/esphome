#include "alarm_control_panel_state.h"
#include "esphome/core/progmem.h"

namespace esphome::alarm_control_panel {

// Alarm control panel state strings indexed by AlarmControlPanelState enum (0-9)
PROGMEM_STRING_TABLE(AlarmControlPanelStateStrings, "DISARMED", "ARMED_HOME", "ARMED_AWAY", "ARMED_NIGHT",
                     "ARMED_VACATION", "ARMED_CUSTOM_BYPASS", "PENDING", "ARMING", "DISARMING", "TRIGGERED", "UNKNOWN");

const LogString *alarm_control_panel_state_to_string(AlarmControlPanelState state) {
  return AlarmControlPanelStateStrings::get_log_str(static_cast<uint8_t>(state),
                                                    AlarmControlPanelStateStrings::LAST_INDEX);
}

}  // namespace esphome::alarm_control_panel
