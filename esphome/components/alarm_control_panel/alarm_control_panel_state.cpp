#include "alarm_control_panel_state.h"

namespace esphome {
namespace alarm_control_panel {

const char *alarm_control_panel_state_to_string(AlarmControlPanelState state) {
  switch (state) {
    case ACP_STATE_DISARMED:
      return "DISARMED";
    case ACP_STATE_ARMED_HOME:
      return "ARMED_HOME";
    case ACP_STATE_ARMED_AWAY:
      return "ARMED_AWAY";
    case ACP_STATE_ARMED_NIGHT:
      return "NIGHT";
    case ACP_STATE_ARMED_VACATION:
      return "ARMED_VACATION";
    case ACP_STATE_ARMED_CUSTOM_BYPASS:
      return "ARMED_CUSTOM_BYPASS";
    case ACP_STATE_PENDING:
      return "PENDING";
    case ACP_STATE_ARMING:
      return "ARMING";
    case ACP_STATE_DISARMING:
      return "DISARMING";
    case ACP_STATE_TRIGGERED:
      return "TRIGGERED";
    default:
      return "UNKNOWN";
  }
}

}  // namespace alarm_control_panel
}  // namespace esphome
