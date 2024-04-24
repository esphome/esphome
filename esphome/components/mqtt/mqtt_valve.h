#pragma once

#include "esphome/core/defines.h"
#include "mqtt_component.h"

#ifdef USE_MQTT
#ifdef USE_VALVE

#include "esphome/components/valve/valve.h"

namespace esphome {
namespace mqtt {

class MQTTValveComponent : public mqtt::MQTTComponent {
 public:
  explicit MQTTValveComponent(valve::Valve *valve);

  void setup() override;
  void send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) override;

  MQTT_COMPONENT_CUSTOM_TOPIC(position, command)
  MQTT_COMPONENT_CUSTOM_TOPIC(position, state)

  bool send_initial_state() override;

  bool publish_state();

  void dump_config() override;

 protected:
  std::string component_type() const override;
  const EntityBase *get_entity() const override;

  valve::Valve *valve_;
};

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
