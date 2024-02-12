#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT
#ifdef USE_DATETIME

#include "esphome/components/datetime/datetime.h"
#include "mqtt_component.h"

namespace esphome {
namespace mqtt {

class MQTTInputDatetimeComponent : public mqtt::MQTTComponent {
 public:
  /** Construct this MQTTInputDatetimeComponent instance with the provided friendly_name and datetime
   *
   * @param datetime The datetime component.
   */
  explicit MQTTInputDatetimeComponent(datetime::InputDatetime *datetime);

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

  datetime::InputDatetime *datetime_;
};

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
