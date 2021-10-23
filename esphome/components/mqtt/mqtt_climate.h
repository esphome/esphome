#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT
#ifdef USE_CLIMATE

#include "esphome/components/climate/climate.h"
#include "mqtt_component.h"

namespace esphome {
namespace mqtt {

class MQTTClimateComponent : public mqtt::MQTTComponent {
 public:
  MQTTClimateComponent(climate::Climate *device);
  void send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) override;
  bool send_initial_state() override;
  std::string component_type() const override;
  void setup() override;

  MQTT_COMPONENT_CUSTOM_TOPIC(current_temperature, state)
  MQTT_COMPONENT_CUSTOM_TOPIC(mode, state)
  MQTT_COMPONENT_CUSTOM_TOPIC(mode, command)
  MQTT_COMPONENT_CUSTOM_TOPIC(target_temperature, state)
  MQTT_COMPONENT_CUSTOM_TOPIC(target_temperature, command)
  MQTT_COMPONENT_CUSTOM_TOPIC(target_temperature_low, state)
  MQTT_COMPONENT_CUSTOM_TOPIC(target_temperature_low, command)
  MQTT_COMPONENT_CUSTOM_TOPIC(target_temperature_high, state)
  MQTT_COMPONENT_CUSTOM_TOPIC(target_temperature_high, command)
  MQTT_COMPONENT_CUSTOM_TOPIC(away, state)
  MQTT_COMPONENT_CUSTOM_TOPIC(away, command)
  MQTT_COMPONENT_CUSTOM_TOPIC(action, state)
  MQTT_COMPONENT_CUSTOM_TOPIC(fan_mode, state)
  MQTT_COMPONENT_CUSTOM_TOPIC(fan_mode, command)
  MQTT_COMPONENT_CUSTOM_TOPIC(swing_mode, state)
  MQTT_COMPONENT_CUSTOM_TOPIC(swing_mode, command)

 protected:
  const EntityBase *get_entity() const override;

  bool publish_state_();

  climate::Climate *device_;
};

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
