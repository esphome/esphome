#include "mqtt_light.h"
#include "esphome/core/log.h"

#ifdef USE_LIGHT

namespace esphome {
namespace mqtt {

static const char *TAG = "mqtt.light";

using namespace esphome::light;

std::string MQTTJSONLightComponent::component_type() const { return "light"; }

void MQTTJSONLightComponent::setup() {
  this->subscribe_json(this->get_command_topic_(), [this](const std::string &topic, JsonObject &root) {
    this->state_->make_call().parse_json(root).perform();
  });

  auto f = std::bind(&MQTTJSONLightComponent::publish_state_, this);
  this->state_->add_new_remote_values_callback([this, f]() { this->defer("send", f); });
}

MQTTJSONLightComponent::MQTTJSONLightComponent(LightState *state) : MQTTComponent(), state_(state) {}

bool MQTTJSONLightComponent::publish_state_() {
  return this->publish_json(this->get_state_topic_(), [this](JsonObject &root) { this->state_->dump_json(root); });
}
LightState *MQTTJSONLightComponent::get_state() const { return this->state_; }
std::string MQTTJSONLightComponent::friendly_name() const { return this->state_->get_name(); }
void MQTTJSONLightComponent::send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) {
  root["schema"] = "json";
  auto traits = this->state_->get_traits();
  if (traits.get_supports_brightness())
    root["brightness"] = true;
  if (traits.get_supports_rgb())
    root["rgb"] = true;
  if (traits.get_supports_color_temperature())
    root["color_temp"] = true;
  if (traits.get_supports_rgb_white_value())
    root["white_value"] = true;
  if (this->state_->supports_effects()) {
    root["effect"] = true;
    JsonArray &effect_list = root.createNestedArray("effect_list");
    for (auto *effect : this->state_->get_effects())
      effect_list.add(effect->get_name());
    effect_list.add("None");
  }
}
bool MQTTJSONLightComponent::send_initial_state() { return this->publish_state_(); }
bool MQTTJSONLightComponent::is_internal() { return this->state_->is_internal(); }
void MQTTJSONLightComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Light '%s':", this->state_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, true)
}

}  // namespace mqtt
}  // namespace esphome

#endif
