#include "mqtt_light.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_JSON
#ifdef USE_MQTT
#ifdef USE_LIGHT

#include "esphome/components/light/light_json_schema.h"
namespace esphome::mqtt {

static const char *const TAG = "mqtt.light";

using namespace esphome::light;

MQTT_COMPONENT_TYPE(MQTTJSONLightComponent, "light")
const EntityBase *MQTTJSONLightComponent::get_entity() const { return this->state_; }

void MQTTJSONLightComponent::setup() {
  this->subscribe_json(this->get_command_topic_(), [this](const std::string &topic, JsonObject root) {
    LightCall call = this->state_->make_call();
    LightJSONSchema::parse_json(*this->state_, call, root);
    call.perform();
  });

  this->state_->add_remote_values_listener(this);
}

void MQTTJSONLightComponent::on_light_remote_values_update() {
  this->defer("send", [this]() { this->publish_state_(); });
}

MQTTJSONLightComponent::MQTTJSONLightComponent(LightState *state) : state_(state) {}

bool MQTTJSONLightComponent::publish_state_() {
  return this->publish_json(this->get_state_topic_(), [this](JsonObject root) {
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
    LightJSONSchema::dump_json(*this->state_, root);
  });
}
LightState *MQTTJSONLightComponent::get_state() const { return this->state_; }

void MQTTJSONLightComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  root[ESPHOME_F("schema")] = ESPHOME_F("json");
  auto traits = this->state_->get_traits();

  root[MQTT_COLOR_MODE] = true;
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  JsonArray color_modes = root[ESPHOME_F("supported_color_modes")].to<JsonArray>();
  if (traits.supports_color_mode(ColorMode::ON_OFF))
    color_modes.add(ESPHOME_F("onoff"));
  if (traits.supports_color_mode(ColorMode::BRIGHTNESS))
    color_modes.add(ESPHOME_F("brightness"));
  if (traits.supports_color_mode(ColorMode::WHITE))
    color_modes.add(ESPHOME_F("white"));
  if (traits.supports_color_mode(ColorMode::COLOR_TEMPERATURE) ||
      traits.supports_color_mode(ColorMode::COLD_WARM_WHITE))
    color_modes.add(ESPHOME_F("color_temp"));
  if (traits.supports_color_mode(ColorMode::RGB))
    color_modes.add(ESPHOME_F("rgb"));
  if (traits.supports_color_mode(ColorMode::RGB_WHITE) ||
      // HA doesn't support RGBCT, and there's no CWWW->CT emulation in ESPHome yet, so ignore CT control for now
      traits.supports_color_mode(ColorMode::RGB_COLOR_TEMPERATURE))
    color_modes.add(ESPHOME_F("rgbw"));
  if (traits.supports_color_mode(ColorMode::RGB_COLD_WARM_WHITE))
    color_modes.add(ESPHOME_F("rgbww"));

  // legacy API
  if (traits.supports_color_capability(ColorCapability::BRIGHTNESS))
    root[ESPHOME_F("brightness")] = true;

  if (traits.supports_color_mode(ColorMode::COLOR_TEMPERATURE) ||
      traits.supports_color_mode(ColorMode::COLD_WARM_WHITE)) {
    root[MQTT_MIN_MIREDS] = traits.get_min_mireds();
    root[MQTT_MAX_MIREDS] = traits.get_max_mireds();
  }

  if (this->state_->supports_effects()) {
    root[ESPHOME_F("effect")] = true;
    JsonArray effect_list = root[MQTT_EFFECT_LIST].to<JsonArray>();
    for (auto *effect : this->state_->get_effects()) {
      // c_str() is safe as effect names are null-terminated strings from codegen
      effect_list.add(effect->get_name().c_str());
    }
    effect_list.add(ESPHOME_F("None"));
  }
}
bool MQTTJSONLightComponent::send_initial_state() { return this->publish_state_(); }
void MQTTJSONLightComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Light '%s':", this->state_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, true)
}

}  // namespace esphome::mqtt

#endif
#endif  // USE_MQTT
#endif  // USE_JSON
