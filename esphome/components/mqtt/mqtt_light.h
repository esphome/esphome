#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT
#ifdef USE_LIGHT

#include "mqtt_component.h"
#include "esphome/components/light/light_state.h"

namespace esphome {
namespace mqtt {

class MQTTJSONLightComponent : public mqtt::MQTTComponent, public light::LightRemoteValuesListener {
 public:
  explicit MQTTJSONLightComponent(light::LightState *state);

  light::LightState *get_state() const;

  void setup() override;

  void dump_config() override;

  void send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) override;

  bool send_initial_state() override;

  // LightRemoteValuesListener interface
  void on_light_remote_values_update() override;

 protected:
  std::string component_type() const override;
  const EntityBase *get_entity() const override;

  bool publish_state_();

  light::LightState *state_;
};

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
