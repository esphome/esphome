#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT
#ifdef USE_INPUT_TEXT

#include "esphome/components/input_text/input_text.h"
#include "mqtt_component.h"

namespace esphome {
namespace mqtt {

class MQTTInputTextComponent : public mqtt::MQTTComponent {
 public:
  /** Construct this MQTTInputTextComponent instance with the provided friendly_name and input_text
   *
   * @param input_text The text input.
   */
  explicit MQTTInputTextComponent(input_text::InputText *input_text);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Override setup.
  void setup() override;
  void dump_config() override;

  void send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) override;

  bool send_initial_state() override;

  bool publish_state(const std::string &value);

 protected:
  /// Override for MQTTComponent, returns "input_text".
  std::string component_type() const override;
  const EntityBase *get_entity() const override;

  input_text::InputText *input_text_;
};

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
