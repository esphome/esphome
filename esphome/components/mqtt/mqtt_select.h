#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT
#ifdef USE_SELECT

#include "esphome/components/select/select.h"
#include "mqtt_component.h"

namespace esphome {
namespace mqtt {

class MQTTSelectComponent : public mqtt::MQTTComponent {
 public:
  /** Construct this MQTTSelectComponent instance with the provided friendly_name and select
   *
   * @param select The select.
   */
  explicit MQTTSelectComponent(select::Select *select);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Override setup.
  void setup() override;
  void dump_config() override;

  void send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) override;

  bool send_initial_state() override;

  bool publish_state(const std::string &value);

 protected:
  /// Override for MQTTComponent, returns "select".
  std::string component_type() const override;
  const EntityBase *get_entity() const override;

  select::Select *select_;
};

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
