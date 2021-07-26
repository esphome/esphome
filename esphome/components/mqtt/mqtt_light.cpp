#include "mqtt_light.h"
#include "esphome/components/light/light_json_schema.h"
#include "esphome/core/log.h"

#ifdef USE_LIGHT

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.light";

using namespace esphome::light;

std::string MQTTJSONLightComponent::component_type() const { return "light"; }

void MQTTJSONLightComponent::setup() {
  this->subscribe_json(this->get_command_topic_(), [this](const std::string &topic, JsonObject &root) {
    LightCall call = this->state_->make_call();
    LightJSONSchema::parse_json(*this->state_, call, root);
    call.perform();
  });

  auto f = std::bind(&MQTTJSONLightComponent::publish_state_, this);
  this->state_->add_new_remote_values_callback([this, f]() { this->defer("send", f); });
}

MQTTJSONLightComponent::MQTTJSONLightComponent(LightState *state) : MQTTComponent(), state_(state) {}

bool MQTTJSONLightComponent::publish_state_() {
  return this->publish_json(this->get_state_topic_(),
                            [this](JsonObject &root) { LightJSONSchema::dump_json(*this->state_, root); });
}
LightState *MQTTJSONLightComponent::get_state() const { return this->state_; }
std::string MQTTJSONLightComponent::friendly_name() const { return this->state_->get_name(); }
void MQTTJSONLightComponent::send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) {
  root["schema"] = "json";
  auto traits = this->state_->get_traits();

  root["color_mode"] = true;
  JsonArray &color_modes = root.createNestedArray("supported_color_modes");
  if (traits.supports_color_mode(ColorMode::COLOR_TEMPERATURE))
    color_modes.add("color_temp");
  if (traits.supports_color_mode(ColorMode::RGB))
    color_modes.add("rgb");
  if (traits.supports_color_mode(ColorMode::RGB_WHITE))
    color_modes.add("rgbw");
  if (traits.supports_color_mode(ColorMode::RGB_COLD_WARM_WHITE))
    color_modes.add("rgbww");
  if (traits.supports_color_mode(ColorMode::BRIGHTNESS))
    color_modes.add("brightness");
  if (traits.supports_color_mode(ColorMode::ON_OFF))
    color_modes.add("onoff");

  // legacy API
  if (traits.supports_color_capability(ColorCapability::BRIGHTNESS))
    root["brightness"] = true;
  if (traits.supports_color_capability(ColorCapability::RGB))
    root["rgb"] = true;
  if (traits.supports_color_capability(ColorCapability::COLOR_TEMPERATURE))
    root["color_temp"] = true;
  if (traits.supports_color_capability(ColorCapability::WHITE))
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
