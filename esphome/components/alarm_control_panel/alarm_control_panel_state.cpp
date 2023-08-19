#include "alarm_control_panel_state.h"

namespace esphome {
namespace alarm_control_panel {

const LogString *alarm_control_panel_state_to_string(AlarmControlPanelState state) {
  switch (state) {
    case ACP_STATE_DISARMED:
      return LOG_STR("DISARMED");
    case ACP_STATE_ARMED_HOME:
      return LOG_STR("ARMED_HOME");
    case ACP_STATE_ARMED_AWAY:
      return LOG_STR("ARMED_AWAY");
    case ACP_STATE_ARMED_NIGHT:
      return LOG_STR("ARMED_NIGHT");
    case ACP_STATE_ARMED_VACATION:
      return LOG_STR("ARMED_VACATION");
    case ACP_STATE_ARMED_CUSTOM_BYPASS:
      return LOG_STR("ARMED_CUSTOM_BYPASS");
    case ACP_STATE_PENDING:
      return LOG_STR("PENDING");
    case ACP_STATE_ARMING:
      return LOG_STR("ARMING");
    case ACP_STATE_DISARMING:
      return LOG_STR("DISARMING");
    case ACP_STATE_TRIGGERED:
      return LOG_STR("TRIGGERED");
    default:
      return LOG_STR("UNKNOWN");
  }
}

}  // namespace alarm_control_panel
}  // namespace esphome
