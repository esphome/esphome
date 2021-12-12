#pragma once

#include "esphome/core/defines.h"
#include "mqtt_component.h"

#ifdef USE_MQTT
#ifdef USE_COVER

#include "esphome/components/cover/cover.h"

namespace esphome {
namespace mqtt {

class MQTTCoverComponent : public mqtt::MQTTComponent {
 public:
  explicit MQTTCoverComponent(cover::Cover *cover);

  void setup() override;
  void send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) override;

  MQTT_COMPONENT_CUSTOM_TOPIC(position, command)
  MQTT_COMPONENT_CUSTOM_TOPIC(position, state)
  MQTT_COMPONENT_CUSTOM_TOPIC(tilt, command)
  MQTT_COMPONENT_CUSTOM_TOPIC(tilt, state)

  bool send_initial_state() override;

  bool publish_state();

  void dump_config() override;

 protected:
  std::string component_type() const override;
  const EntityBase *get_entity() const override;

  cover::Cover *cover_;
};

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
