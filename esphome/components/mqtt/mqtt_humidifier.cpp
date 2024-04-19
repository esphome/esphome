#include "mqtt_humidifier.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_HUMIDIFIER

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.humidifier";

using namespace esphome::humidifier;

void MQTTHumidifierComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  auto traits = this->device_->get_traits();
  // current_humidity_topic
  if (traits.get_supports_current_humidity()) {
    root[MQTT_CURRENT_HUMIDITY_TOPIC] = this->get_current_humidity_state_topic();
  }
  // mode_command_topic
  root[MQTT_MODE_COMMAND_TOPIC] = this->get_mode_command_topic();
  // mode_state_topic
  root[MQTT_MODE_STATE_TOPIC] = this->get_mode_state_topic();
  // modes
  JsonArray modes = root.createNestedArray(MQTT_MODES);
  // sort array for nice UI in HA
  if (traits.supports_mode(HUMIDIFIER_MODE_LEVEL_1))
    modes.add("level 1");
  modes.add("off");
  if (traits.supports_mode(HUMIDIFIER_MODE_LEVEL_2))
  modes.add("level 2");
  if (traits.supports_mode(HUMIDIFIER_MODE_LEVEL_3))
  modes.add("level 3");
  if (traits.supports_mode(HUMIDIFIER_MODE_PRESET))
  modes.add("preset");

  if (traits.get_supports_target_humidity()) {
    // humidity_command_topic
    root[MQTT_HUMIDITY_COMMAND_TOPIC] = this->get_target_humidity_command_topic();
    // humidity_state_topic
    root[MQTT_HUMIDITY_STATE_TOPIC] = this->get_target_humidity_state_topic();
  }

  // min_humidity
  root[MQTT_MIN_HUMI] = traits.get_visual_min_humidity();
  // max_humidity
  root[MQTT_MAX_HUMI] = traits.get_visual_max_humidity();
  // humidity_step
  root["humi_step"] = traits.get_visual_target_humidity_step();
  // humidity units are always coerced to percentage internally
  root[MQTT_HUMIDITY_UNIT] = "%";


  if (traits.get_supports_presets() || !traits.get_supported_custom_presets().empty()) {
    // preset_mode_command_topic
    root[MQTT_PRESET_MODE_COMMAND_TOPIC] = this->get_preset_command_topic();
    // preset_mode_state_topic
    root[MQTT_PRESET_MODE_STATE_TOPIC] = this->get_preset_state_topic();
    // presets
    JsonArray presets = root.createNestedArray("preset_modes");
    if (traits.supports_preset(HUMIDIFIER_PRESET_NONE))
      presets.add("none");
    if (traits.supports_preset(HUMIDIFIER_PRESET_CONSTANT_HUMIDITY))
      presets.add("constant humidity");
    if (traits.supports_preset(HUMIDIFIER_PRESET_BABY))
      presets.add("baby");
    for (const auto &preset : traits.get_supported_custom_presets())
      presets.add(preset);
  }

  if (traits.get_supports_action()) {
    // action_topic
    root[MQTT_ACTION_TOPIC] = this->get_action_state_topic();
  }

  config.state_topic = false;
  config.command_topic = false;
}
void MQTTHumidifierComponent::setup() {
  auto traits = this->device_->get_traits();
  this->subscribe(this->get_mode_command_topic(), [this](const std::string &topic, const std::string &payload) {
    auto call = this->device_->make_call();
    call.set_mode(payload);
    call.perform();
  });

  if (traits.get_supports_target_humidity()) {
    this->subscribe(this->get_target_humidity_command_topic(),
                    [this](const std::string &topic, const std::string &payload) {
                      auto val = parse_number<float>(payload);
                      if (!val.has_value()) {
                        ESP_LOGW(TAG, "Can't convert '%s' to number!", payload.c_str());
                        return;
                      }
                      auto call = this->device_->make_call();
                      call.set_target_humidity(*val);
                      call.perform();
                    });
  }

  if (traits.get_supports_presets() || !traits.get_supported_custom_presets().empty()) {
    this->subscribe(this->get_preset_command_topic(), [this](const std::string &topic, const std::string &payload) {
      auto call = this->device_->make_call();
      call.set_preset(payload);
      call.perform();
    });
  }
  this->device_->add_on_state_callback([this](Humidifier & /*unused*/) { this->publish_state_(); });
}
MQTTHumidifierComponent::MQTTHumidifierComponent(Humidifier *device) : device_(device) {}
bool MQTTHumidifierComponent::send_initial_state() { return this->publish_state_(); }
std::string MQTTHumidifierComponent::component_type() const { return "humidifier"; }
const EntityBase *MQTTHumidifierComponent::get_entity() const { return this->device_; }

bool MQTTHumidifierComponent::publish_state_() {
  auto traits = this->device_->get_traits();
  // mode
  const char *mode_s = "";
  switch (this->device_->mode) {
    case HUMIDIFIER_MODE_OFF:
      mode_s = "off";
      break;
    case HUMIDIFIER_MODE_LEVEL_1:
      mode_s = "level_1";
      break;
    case HUMIDIFIER_MODE_LEVEL_2:
      mode_s = "level_2";
      break;
    case HUMIDIFIER_MODE_LEVEL_3:
      mode_s = "level_3";
      break;
    case HUMIDIFIER_MODE_PRESET:
      mode_s = "preset";
      break;  

  }
  bool success = true;
  if (!this->publish(this->get_mode_state_topic(), mode_s))
    success = false;
  int8_t target_accuracy = traits.get_target_humidity_accuracy_decimals();
  int8_t current_accuracy = traits.get_current_humidity_accuracy_decimals();
  if (traits.get_supports_current_humidity() && !std::isnan(this->device_->current_humidity)) {
    std::string payload = value_accuracy_to_string(this->device_->current_humidity, current_accuracy);
    if (!this->publish(this->get_current_humidity_state_topic(), payload))
      success = false;
  }
  if (traits.get_supports_target_humidity()) {
    std::string payload = value_accuracy_to_string(this->device_->target_humidity, target_accuracy);
    if (!this->publish(this->get_target_humidity_state_topic(), payload))
      success = false;
  }

  if (traits.get_supports_presets() || !traits.get_supported_custom_presets().empty()) {
    std::string payload;
    if (this->device_->preset.has_value()) {
      switch (this->device_->preset.value()) {
        case HUMIDIFIER_PRESET_NONE:
          payload = "none";
          break;
        case HUMIDIFIER_PRESET_CONSTANT_HUMIDITY:
          payload = "constant humidity";
          break;
        case HUMIDIFIER_PRESET_BABY:
          payload = "baby";
          break;
      }
    }
    if (this->device_->custom_preset.has_value())
      payload = this->device_->custom_preset.value();
    if (!this->publish(this->get_preset_state_topic(), payload))
      success = false;
  }

  if (traits.get_supports_action()) {
    const char *payload = "unknown";
    switch (this->device_->action) {
      case HUMIDIFIER_ACTION_OFF:
        payload = "off";
        break;
      case HUMIDIFIER_ACTION_LEVEL_1:
        payload = "level_1";
        break;
      case HUMIDIFIER_ACTION_LEVEL_2:
        payload = "level_2";
        break;
      case HUMIDIFIER_ACTION_LEVEL_3:
        payload = "level_3";
        break;
      case HUMIDIFIER_ACTION_PRESET:
        payload = "preset";
        break;  
    }
    if (!this->publish(this->get_action_state_topic(), payload))
      success = false;
  }
  
  return success;
}

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
