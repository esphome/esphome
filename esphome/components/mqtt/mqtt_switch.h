#pragma once

#include "esphome/core/defines.h"

#ifdef USE_SWITCH

#include "esphome/components/switch/switch.h"
#include "mqtt_component.h"

namespace esphome {
namespace mqtt {

class MQTTSwitchComponent : public mqtt::MQTTComponent {
 public:
  explicit MQTTSwitchComponent(switch_::Switch *a_switch);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;

  void send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) override;

  bool send_initial_state() override;
  bool is_internal() override;

  bool publish_state(bool state);

 protected:
  std::string friendly_name() const override;

  /// "switch" component type.
  std::string component_type() const override;

  switch_::Switch *switch_;
};

}  // namespace mqtt
}  // namespace esphome

#endif
