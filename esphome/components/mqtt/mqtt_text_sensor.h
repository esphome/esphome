#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT
#ifdef USE_TEXT_SENSOR

#include "esphome/components/text_sensor/text_sensor.h"
#include "mqtt_component.h"

namespace esphome::mqtt {

class MQTTTextSensor : public mqtt::MQTTComponent {
 public:
  explicit MQTTTextSensor(text_sensor::TextSensor *sensor);

  void send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) override;

  void setup() override;

  void dump_config() override;

  bool publish_state(const std::string &value);

  bool send_initial_state() override;

 protected:
  const char *component_type() const override;
  const EntityBase *get_entity() const override;

  text_sensor::TextSensor *sensor_;
};

}  // namespace esphome::mqtt

#endif
#endif  // USE_MQTT
