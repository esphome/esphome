#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT
#ifdef USE_BUTTON

#include "esphome/components/button/button.h"
#include "mqtt_component.h"

namespace esphome {
namespace mqtt {

class MQTTButtonComponent : public mqtt::MQTTComponent {
 public:
  explicit MQTTButtonComponent(button::Button *button);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;

  /// Buttons do not send a state so just return true.
  bool send_initial_state() override { return true; }

  void send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) override;

 protected:
  /// "button" component type.
  std::string component_type() const override;
  const EntityBase *get_entity() const override;

  button::Button *button_;
};

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
