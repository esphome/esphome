#pragma once

#include "esphome/core/defines.h"

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

  void send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) override;

  void set_is_status(bool status);

  bool send_initial_state() override;
  bool publish_state(bool state);
  bool is_internal() override;

 protected:
  void do_dump_config() override;

  std::string friendly_name() const override;
  std::string component_type() const override;

  binary_sensor::BinarySensor *binary_sensor_;
};

}  // namespace mqtt
}  // namespace esphome

#endif
