#pragma once

#include "esphome/core/defines.h"
#include "mqtt_component.h"

#ifdef USE_MQTT
#ifdef USE_COVER

#include "esphome/components/cover/cover.h"

namespace esphome::mqtt {

class MQTTCoverComponent : public mqtt::MQTTComponent {
 public:
  explicit MQTTCoverComponent(cover::Cover *cover);

  void setup() override;
  void send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) override;

  MQTT_COMPONENT_CUSTOM_TOPIC(position, command)
  MQTT_COMPONENT_CUSTOM_TOPIC(position, state)
  MQTT_COMPONENT_CUSTOM_TOPIC(tilt, command)
  MQTT_COMPONENT_CUSTOM_TOPIC(tilt, state)

  bool send_initial_state() override;

  bool publish_state();

  void dump_config() override;
#ifdef USE_MQTT_COVER_JSON
  void set_use_json_format(bool use_json_format) { this->use_json_format_ = use_json_format; }
#endif

 protected:
  const char *component_type() const override;
  const EntityBase *get_entity() const override;

  cover::Cover *cover_;
#ifdef USE_MQTT_COVER_JSON
  bool use_json_format_{false};
#endif
};

}  // namespace esphome::mqtt

#endif
#endif  // USE_MQTT
