
#pragma once
#include "esphome/core/automation.h"
#include "alarm_control_panel.h"

namespace esphome {
namespace alarm_control_panel {

class StateTrigger : public Trigger<> {
 public:
  explicit StateTrigger(AlarmControlPanel *alarm_control_panel) {
    alarm_control_panel->add_on_state_callback([this]() { this->trigger(); });
  }
};

}  // namespace alarm_control_panel
}  // namespace esphome
