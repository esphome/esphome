#pragma once

#include "esphome/core/defines.h"

#ifdef USE_LIGHT

#include "mqtt_component.h"
#include "esphome/components/light/light_state.h"

namespace esphome {
namespace mqtt {

class MQTTJSONLightComponent : public mqtt::MQTTComponent {
 public:
  explicit MQTTJSONLightComponent(light::LightState *state);

  light::LightState *get_state() const;

  void setup() override;

  void dump_config() override;

  void send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) override;

  bool send_initial_state() override;

  bool is_internal() override;

 protected:
  std::string friendly_name() const override;
  std::string component_type() const override;

  bool publish_state_();

  light::LightState *state_;
};

}  // namespace mqtt
}  // namespace esphome

#endif
