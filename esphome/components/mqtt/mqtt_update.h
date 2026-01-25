#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT
#ifdef USE_UPDATE

#include "esphome/components/update/update_entity.h"
#include "mqtt_component.h"

namespace esphome::mqtt {

class MQTTUpdateComponent : public mqtt::MQTTComponent {
 public:
  explicit MQTTUpdateComponent(update::UpdateEntity *update);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;

  void send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) override;

  bool send_initial_state() override;

  bool publish_state();

 protected:
  /// "update" component type.
  const char *component_type() const override;
  const EntityBase *get_entity() const override;

  update::UpdateEntity *update_;
};

}  // namespace esphome::mqtt

#endif  // USE_UPDATE
#endif  // USE_MQTT
