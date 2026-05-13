#pragma once

#include "esphome/components/alarm_control_panel/alarm_control_panel.h"
#include "esphome/core/component.h"

namespace esphome::demo {

using namespace alarm_control_panel;

enum class DemoAlarmControlPanelType {
  TYPE_1,
  TYPE_2,
  TYPE_3,
};

class DemoAlarmControlPanel : public AlarmControlPanel, public Component {
 public:
  void setup() override {}

  uint32_t get_supported_features() const override { return ACP_FEAT_ARM_AWAY | ACP_FEAT_TRIGGER; }

  bool get_requires_code() const override { return this->type_ != DemoAlarmControlPanelType::TYPE_1; }

  bool get_requires_code_to_arm() const override { return this->type_ == DemoAlarmControlPanelType::TYPE_3; }

  void set_type(DemoAlarmControlPanelType type) { this->type_ = type; }

 protected:
  void control(const AlarmControlPanelCall &call) override {
    auto state = call.get_state().value_or(ACP_STATE_DISARMED);
    const auto &code = call.get_code();
    switch (state) {
      case ACP_STATE_ARMED_AWAY:
        if (this->get_requires_code_to_arm()) {
          if (!code.has_value() || *code != "1234") {
            this->status_momentary_error("invalid_code", 5000);
            return;
          }
        }
        this->publish_state(ACP_STATE_ARMED_AWAY);
        break;
      case ACP_STATE_DISARMED:
        if (this->get_requires_code()) {
          if (!code.has_value() || *code != "1234") {
            this->status_momentary_error("invalid_code", 5000);
            return;
          }
        }
        this->publish_state(ACP_STATE_DISARMED);
        return;
      case ACP_STATE_TRIGGERED:
        this->publish_state(ACP_STATE_TRIGGERED);
        return;
      case ACP_STATE_PENDING:
        this->publish_state(ACP_STATE_PENDING);
        return;
      default:
        break;
    }
  }
  DemoAlarmControlPanelType type_{};
};

}  // namespace esphome::demo
