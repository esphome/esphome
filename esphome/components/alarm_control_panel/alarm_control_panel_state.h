#pragma once

#include <cstdint>
#include "esphome/core/log.h"

namespace esphome {
namespace alarm_control_panel {

enum AlarmControlPanelState : uint8_t {
  ACP_STATE_DISARMED = 0,
  ACP_STATE_ARMED_HOME = 1,
  ACP_STATE_ARMED_AWAY = 2,
  ACP_STATE_ARMED_NIGHT = 3,
  ACP_STATE_ARMED_VACATION = 4,
  ACP_STATE_ARMED_CUSTOM_BYPASS = 5,
  ACP_STATE_PENDING = 6,
  ACP_STATE_ARMING = 7,
  ACP_STATE_DISARMING = 8,
  ACP_STATE_TRIGGERED = 9
};

/** Returns a string representation of the state.
 *
 * @param state The AlarmControlPanelState.
 */
const LogString *alarm_control_panel_state_to_string(AlarmControlPanelState state);

}  // namespace alarm_control_panel
}  // namespace esphome
