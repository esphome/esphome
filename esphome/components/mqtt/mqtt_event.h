#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT
#ifdef USE_EVENT

#include "esphome/components/event/event.h"
#include "mqtt_component.h"

namespace esphome::mqtt {

class MQTTEventComponent : public mqtt::MQTTComponent {
 public:
  explicit MQTTEventComponent(event::Event *event);

  void send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) override;

  void setup() override;

  void dump_config() override;

  /// Events do not send a state so just return true.
  bool send_initial_state() override { return true; }

 protected:
  bool publish_event_(const std::string &event_type);
  const char *component_type() const override;
  const EntityBase *get_entity() const override;

  event::Event *event_;
};

}  // namespace esphome::mqtt

#endif
#endif  // USE_MQTT
