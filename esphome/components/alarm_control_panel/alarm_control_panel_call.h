#pragma once

#include <string>

#include "alarm_control_panel_state.h"

#include "esphome/core/helpers.h"

namespace esphome {
namespace alarm_control_panel {

class AlarmControlPanel;

class AlarmControlPanelCall {
 public:
  AlarmControlPanelCall(AlarmControlPanel *parent);

  AlarmControlPanelCall &set_code(const std::string &code);
  AlarmControlPanelCall &arm_away();
  AlarmControlPanelCall &arm_home();
  AlarmControlPanelCall &arm_night();
  AlarmControlPanelCall &arm_vacation();
  AlarmControlPanelCall &arm_custom_bypass();
  AlarmControlPanelCall &disarm();
  AlarmControlPanelCall &pending();
  AlarmControlPanelCall &triggered();

  void perform();
  const optional<AlarmControlPanelState> &get_state() const;
  const optional<std::string> &get_code() const;

 protected:
  AlarmControlPanel *parent_;
  optional<std::string> code_{};
  optional<AlarmControlPanelState> state_{};
  void validate_();
};

}  // namespace alarm_control_panel
}  // namespace esphome
