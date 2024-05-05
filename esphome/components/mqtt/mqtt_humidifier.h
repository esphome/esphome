#pragma once
#include "esphome/core/defines.h"

#ifdef USE_MQTT
#ifdef USE_HUMIDIFIER

#include "esphome/components/humidifier/humidifier.h"
#include "mqtt_component.h"

namespace esphome {
namespace mqtt {

class MQTTHumidifierComponent : public mqtt::MQTTComponent {
 public:
  MQTTHumidifierComponent(humidifier::Humidifier *device);
  void send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) override;
  bool send_initial_state() override;
  std::string component_type() const override;
  void setup() override;

  MQTT_COMPONENT_CUSTOM_TOPIC(current_humidity, state)
  MQTT_COMPONENT_CUSTOM_TOPIC(mode, state)
  MQTT_COMPONENT_CUSTOM_TOPIC(mode, command)
  MQTT_COMPONENT_CUSTOM_TOPIC(target_humidity, state)
  MQTT_COMPONENT_CUSTOM_TOPIC(target_humidity, command)
  MQTT_COMPONENT_CUSTOM_TOPIC(target_humidity_low, state)
  MQTT_COMPONENT_CUSTOM_TOPIC(target_humidity_low, command)
  MQTT_COMPONENT_CUSTOM_TOPIC(target_humidity_high, state)
  MQTT_COMPONENT_CUSTOM_TOPIC(target_humidity_high, command)
  MQTT_COMPONENT_CUSTOM_TOPIC(action, state)

 protected:
  const EntityBase *get_entity() const override;

  bool publish_state_();

  humidifier::Humidifier *device_;
};

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
