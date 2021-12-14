#include "mqtt_climate.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_CLIMATE

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.climate";

using namespace esphome::climate;

void MQTTClimateComponent::send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) {
  auto traits = this->device_->get_traits();
  // current_temperature_topic
  if (traits.get_supports_current_temperature()) {
    // current_temperature_topic
    root[MQTT_CURRENT_TEMPERATURE_TOPIC] = this->get_current_temperature_state_topic();
  }
  // mode_command_topic
  root[MQTT_MODE_COMMAND_TOPIC] = this->get_mode_command_topic();
  // mode_state_topic
  root[MQTT_MODE_STATE_TOPIC] = this->get_mode_state_topic();
  // modes
  JsonArray &modes = root.createNestedArray(MQTT_MODES);
  // sort array for nice UI in HA
  if (traits.supports_mode(CLIMATE_MODE_AUTO))
    modes.add("auto");
  modes.add("off");
  if (traits.supports_mode(CLIMATE_MODE_COOL))
    modes.add("cool");
  if (traits.supports_mode(CLIMATE_MODE_HEAT))
    modes.add("heat");
  if (traits.supports_mode(CLIMATE_MODE_FAN_ONLY))
    modes.add("fan_only");
  if (traits.supports_mode(CLIMATE_MODE_DRY))
    modes.add("dry");
  if (traits.supports_mode(CLIMATE_MODE_HEAT_COOL))
    modes.add("heat_cool");

  if (traits.get_supports_two_point_target_temperature()) {
    // temperature_low_command_topic
    root[MQTT_TEMPERATURE_LOW_COMMAND_TOPIC] = this->get_target_temperature_low_command_topic();
    // temperature_low_state_topic
    root[MQTT_TEMPERATURE_LOW_STATE_TOPIC] = this->get_target_temperature_low_state_topic();
    // temperature_high_command_topic
    root[MQTT_TEMPERATURE_HIGH_COMMAND_TOPIC] = this->get_target_temperature_high_command_topic();
    // temperature_high_state_topic
    root[MQTT_TEMPERATURE_HIGH_STATE_TOPIC] = this->get_target_temperature_high_state_topic();
  } else {
    // temperature_command_topic
    root[MQTT_TEMPERATURE_COMMAND_TOPIC] = this->get_target_temperature_command_topic();
    // temperature_state_topic
    root[MQTT_TEMPERATURE_STATE_TOPIC] = this->get_target_temperature_state_topic();
  }

  // min_temp
  root[MQTT_MIN_TEMP] = traits.get_visual_min_temperature();
  // max_temp
  root[MQTT_MAX_TEMP] = traits.get_visual_max_temperature();
  // temp_step
  root["temp_step"] = traits.get_visual_temperature_step();
  // temperature units are always coerced to Celsius internally
  root[MQTT_TEMPERATURE_UNIT] = "C";

  if (traits.supports_preset(CLIMATE_PRESET_AWAY)) {
    // away_mode_command_topic
    root[MQTT_AWAY_MODE_COMMAND_TOPIC] = this->get_away_command_topic();
    // away_mode_state_topic
    root[MQTT_AWAY_MODE_STATE_TOPIC] = this->get_away_state_topic();
  }
  if (traits.get_supports_action()) {
    // action_topic
    root[MQTT_ACTION_TOPIC] = this->get_action_state_topic();
  }

  if (traits.get_supports_fan_modes() || !traits.get_supported_custom_fan_modes().empty()) {
    // fan_mode_command_topic
    root[MQTT_FAN_MODE_COMMAND_TOPIC] = this->get_fan_mode_command_topic();
    // fan_mode_state_topic
    root[MQTT_FAN_MODE_STATE_TOPIC] = this->get_fan_mode_state_topic();
    // fan_modes
    JsonArray &fan_modes = root.createNestedArray("fan_modes");
    if (traits.supports_fan_mode(CLIMATE_FAN_ON))
      fan_modes.add("on");
    if (traits.supports_fan_mode(CLIMATE_FAN_OFF))
      fan_modes.add("off");
    if (traits.supports_fan_mode(CLIMATE_FAN_AUTO))
      fan_modes.add("auto");
    if (traits.supports_fan_mode(CLIMATE_FAN_LOW))
      fan_modes.add("low");
    if (traits.supports_fan_mode(CLIMATE_FAN_MEDIUM))
      fan_modes.add("medium");
    if (traits.supports_fan_mode(CLIMATE_FAN_HIGH))
      fan_modes.add("high");
    if (traits.supports_fan_mode(CLIMATE_FAN_MIDDLE))
      fan_modes.add("middle");
    if (traits.supports_fan_mode(CLIMATE_FAN_FOCUS))
      fan_modes.add("focus");
    if (traits.supports_fan_mode(CLIMATE_FAN_DIFFUSE))
      fan_modes.add("diffuse");
    for (const auto &fan_mode : traits.get_supported_custom_fan_modes())
      fan_modes.add(fan_mode);
  }

  if (traits.get_supports_swing_modes()) {
    // swing_mode_command_topic
    root[MQTT_SWING_MODE_COMMAND_TOPIC] = this->get_swing_mode_command_topic();
    // swing_mode_state_topic
    root[MQTT_SWING_MODE_STATE_TOPIC] = this->get_swing_mode_state_topic();
    // swing_modes
    JsonArray &swing_modes = root.createNestedArray("swing_modes");
    if (traits.supports_swing_mode(CLIMATE_SWING_OFF))
      swing_modes.add("off");
    if (traits.supports_swing_mode(CLIMATE_SWING_BOTH))
      swing_modes.add("both");
    if (traits.supports_swing_mode(CLIMATE_SWING_VERTICAL))
      swing_modes.add("vertical");
    if (traits.supports_swing_mode(CLIMATE_SWING_HORIZONTAL))
      swing_modes.add("horizontal");
  }

  config.state_topic = false;
  config.command_topic = false;
}
void MQTTClimateComponent::setup() {
  auto traits = this->device_->get_traits();
  this->subscribe(this->get_mode_command_topic(), [this](const std::string &topic, const std::string &payload) {
    auto call = this->device_->make_call();
    call.set_mode(payload);
    call.perform();
  });

  if (traits.get_supports_two_point_target_temperature()) {
    this->subscribe(this->get_target_temperature_low_command_topic(),
                    [this](const std::string &topic, const std::string &payload) {
                      auto val = parse_number<float>(payload);
                      if (!val.has_value()) {
                        ESP_LOGW(TAG, "Can't convert '%s' to number!", payload.c_str());
                        return;
                      }
                      auto call = this->device_->make_call();
                      call.set_target_temperature_low(*val);
                      call.perform();
                    });
    this->subscribe(this->get_target_temperature_high_command_topic(),
                    [this](const std::string &topic, const std::string &payload) {
                      auto val = parse_number<float>(payload);
                      if (!val.has_value()) {
                        ESP_LOGW(TAG, "Can't convert '%s' to number!", payload.c_str());
                        return;
                      }
                      auto call = this->device_->make_call();
                      call.set_target_temperature_high(*val);
                      call.perform();
                    });
  } else {
    this->subscribe(this->get_target_temperature_command_topic(),
                    [this](const std::string &topic, const std::string &payload) {
                      auto val = parse_number<float>(payload);
                      if (!val.has_value()) {
                        ESP_LOGW(TAG, "Can't convert '%s' to number!", payload.c_str());
                        return;
                      }
                      auto call = this->device_->make_call();
                      call.set_target_temperature(*val);
                      call.perform();
                    });
  }

  if (traits.supports_preset(CLIMATE_PRESET_AWAY)) {
    this->subscribe(this->get_away_command_topic(), [this](const std::string &topic, const std::string &payload) {
      auto onoff = parse_on_off(payload.c_str());
      auto call = this->device_->make_call();
      switch (onoff) {
        case PARSE_ON:
          call.set_preset(CLIMATE_PRESET_AWAY);
          break;
        case PARSE_OFF:
          call.set_preset(CLIMATE_PRESET_HOME);
          break;
        case PARSE_TOGGLE:
          call.set_preset(this->device_->preset == CLIMATE_PRESET_AWAY ? CLIMATE_PRESET_HOME : CLIMATE_PRESET_AWAY);
          break;
        case PARSE_NONE:
        default:
          ESP_LOGW(TAG, "Unknown payload '%s'", payload.c_str());
          return;
      }
      call.perform();
    });
  }

  if (traits.get_supports_fan_modes()) {
    this->subscribe(this->get_fan_mode_command_topic(), [this](const std::string &topic, const std::string &payload) {
      auto call = this->device_->make_call();
      call.set_fan_mode(payload);
      call.perform();
    });
  }

  if (traits.get_supports_swing_modes()) {
    this->subscribe(this->get_swing_mode_command_topic(), [this](const std::string &topic, const std::string &payload) {
      auto call = this->device_->make_call();
      call.set_swing_mode(payload);
      call.perform();
    });
  }

  this->device_->add_on_state_callback([this]() { this->publish_state_(); });
}
MQTTClimateComponent::MQTTClimateComponent(Climate *device) : device_(device) {}
bool MQTTClimateComponent::send_initial_state() { return this->publish_state_(); }
std::string MQTTClimateComponent::component_type() const { return "climate"; }
const EntityBase *MQTTClimateComponent::get_entity() const { return this->device_; }

bool MQTTClimateComponent::publish_state_() {
  auto traits = this->device_->get_traits();
  // mode
  const char *mode_s = "";
  switch (this->device_->mode) {
    case CLIMATE_MODE_OFF:
      mode_s = "off";
      break;
    case CLIMATE_MODE_AUTO:
      mode_s = "auto";
      break;
    case CLIMATE_MODE_COOL:
      mode_s = "cool";
      break;
    case CLIMATE_MODE_HEAT:
      mode_s = "heat";
      break;
    case CLIMATE_MODE_FAN_ONLY:
      mode_s = "fan_only";
      break;
    case CLIMATE_MODE_DRY:
      mode_s = "dry";
      break;
    case CLIMATE_MODE_HEAT_COOL:
      mode_s = "heat_cool";
      break;
  }
  bool success = true;
  if (!this->publish(this->get_mode_state_topic(), mode_s))
    success = false;
  int8_t accuracy = traits.get_temperature_accuracy_decimals();
  if (traits.get_supports_current_temperature() && !std::isnan(this->device_->current_temperature)) {
    std::string payload = value_accuracy_to_string(this->device_->current_temperature, accuracy);
    if (!this->publish(this->get_current_temperature_state_topic(), payload))
      success = false;
  }
  if (traits.get_supports_two_point_target_temperature()) {
    std::string payload = value_accuracy_to_string(this->device_->target_temperature_low, accuracy);
    if (!this->publish(this->get_target_temperature_low_state_topic(), payload))
      success = false;
    payload = value_accuracy_to_string(this->device_->target_temperature_high, accuracy);
    if (!this->publish(this->get_target_temperature_high_state_topic(), payload))
      success = false;
  } else {
    std::string payload = value_accuracy_to_string(this->device_->target_temperature, accuracy);
    if (!this->publish(this->get_target_temperature_state_topic(), payload))
      success = false;
  }

  if (traits.supports_preset(CLIMATE_PRESET_AWAY)) {
    std::string payload = ONOFF(this->device_->preset == CLIMATE_PRESET_AWAY);
    if (!this->publish(this->get_away_state_topic(), payload))
      success = false;
  }
  if (traits.get_supports_action()) {
    const char *payload = "unknown";
    switch (this->device_->action) {
      case CLIMATE_ACTION_OFF:
        payload = "off";
        break;
      case CLIMATE_ACTION_COOLING:
        payload = "cooling";
        break;
      case CLIMATE_ACTION_HEATING:
        payload = "heating";
        break;
      case CLIMATE_ACTION_IDLE:
        payload = "idle";
        break;
      case CLIMATE_ACTION_DRYING:
        payload = "drying";
        break;
      case CLIMATE_ACTION_FAN:
        payload = "fan";
        break;
    }
    if (!this->publish(this->get_action_state_topic(), payload))
      success = false;
  }

  if (traits.get_supports_fan_modes()) {
    std::string payload;
    if (this->device_->fan_mode.has_value())
      switch (this->device_->fan_mode.value()) {
        case CLIMATE_FAN_ON:
          payload = "on";
          break;
        case CLIMATE_FAN_OFF:
          payload = "off";
          break;
        case CLIMATE_FAN_AUTO:
          payload = "auto";
          break;
        case CLIMATE_FAN_LOW:
          payload = "low";
          break;
        case CLIMATE_FAN_MEDIUM:
          payload = "medium";
          break;
        case CLIMATE_FAN_HIGH:
          payload = "high";
          break;
        case CLIMATE_FAN_MIDDLE:
          payload = "middle";
          break;
        case CLIMATE_FAN_FOCUS:
          payload = "focus";
          break;
        case CLIMATE_FAN_DIFFUSE:
          payload = "diffuse";
          break;
      }
    if (this->device_->custom_fan_mode.has_value())
      payload = this->device_->custom_fan_mode.value();
    if (!this->publish(this->get_fan_mode_state_topic(), payload))
      success = false;
  }

  if (traits.get_supports_swing_modes()) {
    const char *payload = "";
    switch (this->device_->swing_mode) {
      case CLIMATE_SWING_OFF:
        payload = "off";
        break;
      case CLIMATE_SWING_BOTH:
        payload = "both";
        break;
      case CLIMATE_SWING_VERTICAL:
        payload = "vertical";
        break;
      case CLIMATE_SWING_HORIZONTAL:
        payload = "horizontal";
        break;
    }
    if (!this->publish(this->get_swing_mode_state_topic(), payload))
      success = false;
  }

  return success;
}

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
