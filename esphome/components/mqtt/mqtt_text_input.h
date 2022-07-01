#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT
#ifdef USE_TEXT_INPUT

#include "esphome/components/text_input/text_input.h"
#include "mqtt_component.h"

namespace esphome {
namespace mqtt {

class MQTTTextInputComponent : public mqtt::MQTTComponent {
 public:
  /** Construct this MQTTTextInputComponent instance with the provided friendly_name and text_input
   *
   * @param text_input The text input.
   */
  explicit MQTTTextInputComponent(text_input::TextInput *text_input);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Override setup.
  void setup() override;
  void dump_config() override;

  void send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) override;

  bool send_initial_state() override;

  bool publish_state(std::string value);

 protected:
  /// Override for MQTTComponent, returns "text_input".
  std::string component_type() const override;
  const EntityBase *get_entity() const override;

  text_input::TextInput *text_input;
};

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
