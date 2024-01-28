#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT
#ifdef USE_INPUT_DATETIME

#include "esphome/components/input_datetime/input_datetime.h"
#include "mqtt_component.h"

namespace esphome {
namespace mqtt {

class MQTTInputDatetimeComponent : public mqtt::MQTTComponent {
 public:
  /** Construct this MQTTInputDatetimeComponent instance with the provided friendly_name and input_datetime
   *
   * @param input_datetime The input_datetime component.
   */
  explicit MQTTInputDatetimeComponent(input_datetime::InputDatetime *input_datetime);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Override setup.
  void setup() override;
  void dump_config() override;

  void send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) override;

  bool send_initial_state() override;

  bool publish_state(std::string value);

 protected:
  /// Override for MQTTComponent, returns "number".
  std::string component_type() const override;
  const EntityBase *get_entity() const override;

  input_datetime::InputDatetime *input_datetime_;
};

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
