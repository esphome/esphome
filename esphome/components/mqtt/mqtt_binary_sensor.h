#pragma once

#include "esphome/core/defines.h"
#ifdef USE_MQTT
#ifdef USE_BINARY_SENSOR

#include "mqtt_component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace mqtt {

class MQTTBinarySensorComponent : public mqtt::MQTTComponent {
 public:
  /** Construct a MQTTBinarySensorComponent.
   *
   * @param binary_sensor The binary sensor.
   */
  explicit MQTTBinarySensorComponent(binary_sensor::BinarySensor *binary_sensor);

  void setup() override;

  void dump_config() override;

  void send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) override;

  void set_is_status(bool status);

  bool send_initial_state() override;
  bool publish_state(bool state);

 protected:
  std::string component_type() const override;
  const EntityBase *get_entity() const override;

  binary_sensor::BinarySensor *binary_sensor_;
};

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
