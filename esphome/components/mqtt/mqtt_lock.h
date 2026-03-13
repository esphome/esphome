#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT
#ifdef USE_LOCK

#include "esphome/components/lock/lock.h"
#include "mqtt_component.h"

namespace esphome::mqtt {

class MQTTLockComponent : public mqtt::MQTTComponent {
 public:
  explicit MQTTLockComponent(lock::Lock *a_lock);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;

  void send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) override;

  bool send_initial_state() override;

  bool publish_state();

 protected:
  /// "lock" component type.
  const char *component_type() const override;
  const EntityBase *get_entity() const override;

  lock::Lock *lock_;
};

}  // namespace esphome::mqtt

#endif
#endif  // USE_MQTT
