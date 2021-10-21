#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT
#ifdef USE_TEXT_SENSOR

#include "esphome/components/text_sensor/text_sensor.h"
#include "mqtt_component.h"

namespace esphome {
namespace mqtt {

class MQTTTextSensor : public mqtt::MQTTComponent {
 public:
  explicit MQTTTextSensor(text_sensor::TextSensor *sensor);

  void send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) override;

  void setup() override;

  void dump_config() override;

  bool publish_state(const std::string &value);

  bool send_initial_state() override;

 protected:
  std::string component_type() const override;
  const EntityBase *get_entity() const override;
  std::string unique_id() override;

  text_sensor::TextSensor *sensor_;
};

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
