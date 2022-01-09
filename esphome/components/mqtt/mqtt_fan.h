#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT
#ifdef USE_FAN

#include "esphome/components/fan/fan_state.h"
#include "mqtt_component.h"

namespace esphome {
namespace mqtt {

class MQTTFanComponent : public mqtt::MQTTComponent {
 public:
  explicit MQTTFanComponent(fan::Fan *state);

  MQTT_COMPONENT_CUSTOM_TOPIC(oscillation, command)
  MQTT_COMPONENT_CUSTOM_TOPIC(oscillation, state)
  MQTT_COMPONENT_CUSTOM_TOPIC(speed_level, command)
  MQTT_COMPONENT_CUSTOM_TOPIC(speed_level, state)
  MQTT_COMPONENT_CUSTOM_TOPIC(speed, command)
  MQTT_COMPONENT_CUSTOM_TOPIC(speed, state)

  void send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) override;

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Setup the fan subscriptions and discovery.
  void setup() override;

  void dump_config() override;

  /// Send the full current state to MQTT.
  bool send_initial_state() override;
  bool publish_state();
  /// 'fan' component type for discovery.
  std::string component_type() const override;

  fan::Fan *get_state() const;

 protected:
  const EntityBase *get_entity() const override;

  fan::Fan *state_;
};

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
