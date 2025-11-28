#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT
#ifdef USE_ALARM_CONTROL_PANEL

#include "mqtt_component.h"
#include "esphome/components/alarm_control_panel/alarm_control_panel.h"

namespace esphome {
namespace mqtt {

class MQTTAlarmControlPanelComponent final : public mqtt::MQTTComponent,
                                             public alarm_control_panel::AlarmControlPanelListener {
 public:
  explicit MQTTAlarmControlPanelComponent(alarm_control_panel::AlarmControlPanel *alarm_control_panel);

  void setup() override;

  void send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) override;

  bool send_initial_state() override;

  bool publish_state();

  void dump_config() override;

  // AlarmControlPanelListener interface
  void on_state(alarm_control_panel::AlarmControlPanelState new_state,
                alarm_control_panel::AlarmControlPanelState prev_state) override {
    this->publish_state();
  }

 protected:
  std::string component_type() const override;
  const EntityBase *get_entity() const override;

  alarm_control_panel::AlarmControlPanel *alarm_control_panel_;
};

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
