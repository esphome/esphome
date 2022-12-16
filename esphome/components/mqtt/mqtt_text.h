#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT
#ifdef USE_TEXT

#include "esphome/components/text/text.h"
#include "mqtt_component.h"

namespace esphome {
namespace mqtt {

class MQTTTextComponent : public mqtt::MQTTComponent {
 public:
  /** Construct this MQTTTextComponent instance with the provided friendly_name and text
   *
   * @param text The text input.
   */
  explicit MQTTTextComponent(text::Text *text);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Override setup.
  void setup() override;
  void dump_config() override;

  void send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) override;

  bool send_initial_state() override;

  bool publish_state(const std::string &value);

 protected:
  /// Override for MQTTComponent, returns "text".
  std::string component_type() const override;
  const EntityBase *get_entity() const override;

  text::Text *text_;
};

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
