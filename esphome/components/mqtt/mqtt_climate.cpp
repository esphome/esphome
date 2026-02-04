#include "mqtt_climate.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_CLIMATE

namespace esphome::mqtt {

static const char *const TAG = "mqtt.climate";

using namespace esphome::climate;

static ProgmemStr climate_mode_to_mqtt_str(ClimateMode mode) {
  switch (mode) {
    case CLIMATE_MODE_OFF:
      return ESPHOME_F("off");
    case CLIMATE_MODE_HEAT_COOL:
      return ESPHOME_F("heat_cool");
    case CLIMATE_MODE_AUTO:
      return ESPHOME_F("auto");
    case CLIMATE_MODE_COOL:
      return ESPHOME_F("cool");
    case CLIMATE_MODE_HEAT:
      return ESPHOME_F("heat");
    case CLIMATE_MODE_FAN_ONLY:
      return ESPHOME_F("fan_only");
    case CLIMATE_MODE_DRY:
      return ESPHOME_F("dry");
    default:
      return ESPHOME_F("unknown");
  }
}

static ProgmemStr climate_action_to_mqtt_str(ClimateAction action) {
  switch (action) {
    case CLIMATE_ACTION_OFF:
      return ESPHOME_F("off");
    case CLIMATE_ACTION_COOLING:
      return ESPHOME_F("cooling");
    case CLIMATE_ACTION_HEATING:
      return ESPHOME_F("heating");
    case CLIMATE_ACTION_IDLE:
      return ESPHOME_F("idle");
    case CLIMATE_ACTION_DRYING:
      return ESPHOME_F("drying");
    case CLIMATE_ACTION_FAN:
      return ESPHOME_F("fan");
    default:
      return ESPHOME_F("unknown");
  }
}

static ProgmemStr climate_fan_mode_to_mqtt_str(ClimateFanMode fan_mode) {
  switch (fan_mode) {
    case CLIMATE_FAN_ON:
      return ESPHOME_F("on");
    case CLIMATE_FAN_OFF:
      return ESPHOME_F("off");
    case CLIMATE_FAN_AUTO:
      return ESPHOME_F("auto");
    case CLIMATE_FAN_LOW:
      return ESPHOME_F("low");
    case CLIMATE_FAN_MEDIUM:
      return ESPHOME_F("medium");
    case CLIMATE_FAN_HIGH:
      return ESPHOME_F("high");
    case CLIMATE_FAN_MIDDLE:
      return ESPHOME_F("middle");
    case CLIMATE_FAN_FOCUS:
      return ESPHOME_F("focus");
    case CLIMATE_FAN_DIFFUSE:
      return ESPHOME_F("diffuse");
    case CLIMATE_FAN_QUIET:
      return ESPHOME_F("quiet");
    default:
      return ESPHOME_F("unknown");
  }
}

static ProgmemStr climate_swing_mode_to_mqtt_str(ClimateSwingMode swing_mode) {
  switch (swing_mode) {
    case CLIMATE_SWING_OFF:
      return ESPHOME_F("off");
    case CLIMATE_SWING_BOTH:
      return ESPHOME_F("both");
    case CLIMATE_SWING_VERTICAL:
      return ESPHOME_F("vertical");
    case CLIMATE_SWING_HORIZONTAL:
      return ESPHOME_F("horizontal");
    default:
      return ESPHOME_F("unknown");
  }
}

static ProgmemStr climate_preset_to_mqtt_str(ClimatePreset preset) {
  switch (preset) {
    case CLIMATE_PRESET_NONE:
      return ESPHOME_F("none");
    case CLIMATE_PRESET_HOME:
      return ESPHOME_F("home");
    case CLIMATE_PRESET_ECO:
      return ESPHOME_F("eco");
    case CLIMATE_PRESET_AWAY:
      return ESPHOME_F("away");
    case CLIMATE_PRESET_BOOST:
      return ESPHOME_F("boost");
    case CLIMATE_PRESET_COMFORT:
      return ESPHOME_F("comfort");
    case CLIMATE_PRESET_SLEEP:
      return ESPHOME_F("sleep");
    case CLIMATE_PRESET_ACTIVITY:
      return ESPHOME_F("activity");
    default:
      return ESPHOME_F("unknown");
  }
}

void MQTTClimateComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  auto traits = this->device_->get_traits();
  // current_temperature_topic
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE)) {
    root[MQTT_CURRENT_TEMPERATURE_TOPIC] = this->get_current_temperature_state_topic();
  }
  // current_humidity_topic
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY)) {
    root[MQTT_CURRENT_HUMIDITY_TOPIC] = this->get_current_humidity_state_topic();
  }
  // mode_command_topic
  root[MQTT_MODE_COMMAND_TOPIC] = this->get_mode_command_topic();
  // mode_state_topic
  root[MQTT_MODE_STATE_TOPIC] = this->get_mode_state_topic();
  // modes
  JsonArray modes = root[MQTT_MODES].to<JsonArray>();
  // sort array for nice UI in HA
  if (traits.supports_mode(CLIMATE_MODE_AUTO))
    modes.add(ESPHOME_F("auto"));
  modes.add(ESPHOME_F("off"));
  if (traits.supports_mode(CLIMATE_MODE_COOL))
    modes.add(ESPHOME_F("cool"));
  if (traits.supports_mode(CLIMATE_MODE_HEAT))
    modes.add(ESPHOME_F("heat"));
  if (traits.supports_mode(CLIMATE_MODE_FAN_ONLY))
    modes.add(ESPHOME_F("fan_only"));
  if (traits.supports_mode(CLIMATE_MODE_DRY))
    modes.add(ESPHOME_F("dry"));
  if (traits.supports_mode(CLIMATE_MODE_HEAT_COOL))
    modes.add(ESPHOME_F("heat_cool"));

  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE |
                               climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE)) {
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

  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY)) {
    // target_humidity_command_topic
    root[MQTT_TARGET_HUMIDITY_COMMAND_TOPIC] = this->get_target_humidity_command_topic();
    // target_humidity_state_topic
    root[MQTT_TARGET_HUMIDITY_STATE_TOPIC] = this->get_target_humidity_state_topic();
  }

  // min_temp
  root[MQTT_MIN_TEMP] = traits.get_visual_min_temperature();
  // max_temp
  root[MQTT_MAX_TEMP] = traits.get_visual_max_temperature();
  // target_temp_step
  root[MQTT_TARGET_TEMPERATURE_STEP] = roundf(traits.get_visual_target_temperature_step() * 10) * 0.1;
  // current_temp_step
  root[MQTT_CURRENT_TEMPERATURE_STEP] = roundf(traits.get_visual_current_temperature_step() * 10) * 0.1;
  // temperature units are always coerced to Celsius internally
  root[MQTT_TEMPERATURE_UNIT] = "C";

  // min_humidity
  root[MQTT_MIN_HUMIDITY] = traits.get_visual_min_humidity();
  // max_humidity
  root[MQTT_MAX_HUMIDITY] = traits.get_visual_max_humidity();

  if (traits.get_supports_presets() || !traits.get_supported_custom_presets().empty()) {
    // preset_mode_command_topic
    root[MQTT_PRESET_MODE_COMMAND_TOPIC] = this->get_preset_command_topic();
    // preset_mode_state_topic
    root[MQTT_PRESET_MODE_STATE_TOPIC] = this->get_preset_state_topic();
    // presets
    JsonArray presets = root[ESPHOME_F("preset_modes")].to<JsonArray>();
    if (traits.supports_preset(CLIMATE_PRESET_HOME))
      presets.add(ESPHOME_F("home"));
    if (traits.supports_preset(CLIMATE_PRESET_AWAY))
      presets.add(ESPHOME_F("away"));
    if (traits.supports_preset(CLIMATE_PRESET_BOOST))
      presets.add(ESPHOME_F("boost"));
    if (traits.supports_preset(CLIMATE_PRESET_COMFORT))
      presets.add(ESPHOME_F("comfort"));
    if (traits.supports_preset(CLIMATE_PRESET_ECO))
      presets.add(ESPHOME_F("eco"));
    if (traits.supports_preset(CLIMATE_PRESET_SLEEP))
      presets.add(ESPHOME_F("sleep"));
    if (traits.supports_preset(CLIMATE_PRESET_ACTIVITY))
      presets.add(ESPHOME_F("activity"));
    for (const auto &preset : traits.get_supported_custom_presets())
      presets.add(preset);
  }

  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_ACTION)) {
    // action_topic
    root[MQTT_ACTION_TOPIC] = this->get_action_state_topic();
  }

  if (traits.get_supports_fan_modes()) {
    // fan_mode_command_topic
    root[MQTT_FAN_MODE_COMMAND_TOPIC] = this->get_fan_mode_command_topic();
    // fan_mode_state_topic
    root[MQTT_FAN_MODE_STATE_TOPIC] = this->get_fan_mode_state_topic();
    // fan_modes
    JsonArray fan_modes = root[ESPHOME_F("fan_modes")].to<JsonArray>();
    if (traits.supports_fan_mode(CLIMATE_FAN_ON))
      fan_modes.add(ESPHOME_F("on"));
    if (traits.supports_fan_mode(CLIMATE_FAN_OFF))
      fan_modes.add(ESPHOME_F("off"));
    if (traits.supports_fan_mode(CLIMATE_FAN_AUTO))
      fan_modes.add(ESPHOME_F("auto"));
    if (traits.supports_fan_mode(CLIMATE_FAN_LOW))
      fan_modes.add(ESPHOME_F("low"));
    if (traits.supports_fan_mode(CLIMATE_FAN_MEDIUM))
      fan_modes.add(ESPHOME_F("medium"));
    if (traits.supports_fan_mode(CLIMATE_FAN_HIGH))
      fan_modes.add(ESPHOME_F("high"));
    if (traits.supports_fan_mode(CLIMATE_FAN_MIDDLE))
      fan_modes.add(ESPHOME_F("middle"));
    if (traits.supports_fan_mode(CLIMATE_FAN_FOCUS))
      fan_modes.add(ESPHOME_F("focus"));
    if (traits.supports_fan_mode(CLIMATE_FAN_DIFFUSE))
      fan_modes.add(ESPHOME_F("diffuse"));
    if (traits.supports_fan_mode(CLIMATE_FAN_QUIET))
      fan_modes.add(ESPHOME_F("quiet"));
    for (const auto &fan_mode : traits.get_supported_custom_fan_modes())
      fan_modes.add(fan_mode);
  }

  if (traits.get_supports_swing_modes()) {
    // swing_mode_command_topic
    root[MQTT_SWING_MODE_COMMAND_TOPIC] = this->get_swing_mode_command_topic();
    // swing_mode_state_topic
    root[MQTT_SWING_MODE_STATE_TOPIC] = this->get_swing_mode_state_topic();
    // swing_modes
    JsonArray swing_modes = root[ESPHOME_F("swing_modes")].to<JsonArray>();
    if (traits.supports_swing_mode(CLIMATE_SWING_OFF))
      swing_modes.add(ESPHOME_F("off"));
    if (traits.supports_swing_mode(CLIMATE_SWING_BOTH))
      swing_modes.add(ESPHOME_F("both"));
    if (traits.supports_swing_mode(CLIMATE_SWING_VERTICAL))
      swing_modes.add(ESPHOME_F("vertical"));
    if (traits.supports_swing_mode(CLIMATE_SWING_HORIZONTAL))
      swing_modes.add(ESPHOME_F("horizontal"));
  }

  config.state_topic = false;
  config.command_topic = false;
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}
void MQTTClimateComponent::setup() {
  auto traits = this->device_->get_traits();
  this->subscribe(this->get_mode_command_topic(), [this](const std::string &topic, const std::string &payload) {
    auto call = this->device_->make_call();
    call.set_mode(payload);
    call.perform();
  });

  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE |
                               climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE)) {
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

  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY)) {
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

  this->device_->add_on_state_callback([this](Climate & /*unused*/) { this->publish_state_(); });
}
MQTTClimateComponent::MQTTClimateComponent(Climate *device) : device_(device) {}
bool MQTTClimateComponent::send_initial_state() { return this->publish_state_(); }
MQTT_COMPONENT_TYPE(MQTTClimateComponent, "climate")
const EntityBase *MQTTClimateComponent::get_entity() const { return this->device_; }

bool MQTTClimateComponent::publish_state_() {
  auto traits = this->device_->get_traits();
  // mode
  bool success = true;
  if (!this->publish(this->get_mode_state_topic(), climate_mode_to_mqtt_str(this->device_->mode)))
    success = false;
  int8_t target_accuracy = traits.get_target_temperature_accuracy_decimals();
  int8_t current_accuracy = traits.get_current_temperature_accuracy_decimals();
  char payload[VALUE_ACCURACY_MAX_LEN];
  size_t len;
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE) &&
      !std::isnan(this->device_->current_temperature)) {
    len = value_accuracy_to_buf(payload, this->device_->current_temperature, current_accuracy);
    if (!this->publish(this->get_current_temperature_state_topic(), payload, len))
      success = false;
  }
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE |
                               climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE)) {
    len = value_accuracy_to_buf(payload, this->device_->target_temperature_low, target_accuracy);
    if (!this->publish(this->get_target_temperature_low_state_topic(), payload, len))
      success = false;
    len = value_accuracy_to_buf(payload, this->device_->target_temperature_high, target_accuracy);
    if (!this->publish(this->get_target_temperature_high_state_topic(), payload, len))
      success = false;
  } else {
    len = value_accuracy_to_buf(payload, this->device_->target_temperature, target_accuracy);
    if (!this->publish(this->get_target_temperature_state_topic(), payload, len))
      success = false;
  }

  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY) &&
      !std::isnan(this->device_->current_humidity)) {
    len = value_accuracy_to_buf(payload, this->device_->current_humidity, 0);
    if (!this->publish(this->get_current_humidity_state_topic(), payload, len))
      success = false;
  }
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY) &&
      !std::isnan(this->device_->target_humidity)) {
    len = value_accuracy_to_buf(payload, this->device_->target_humidity, 0);
    if (!this->publish(this->get_target_humidity_state_topic(), payload, len))
      success = false;
  }

  if (traits.get_supports_presets() || !traits.get_supported_custom_presets().empty()) {
    if (this->device_->has_custom_preset()) {
      if (!this->publish(this->get_preset_state_topic(), this->device_->get_custom_preset()))
        success = false;
    } else if (this->device_->preset.has_value()) {
      if (!this->publish(this->get_preset_state_topic(), climate_preset_to_mqtt_str(this->device_->preset.value())))
        success = false;
    } else if (!this->publish(this->get_preset_state_topic(), "")) {
      success = false;
    }
  }

  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_ACTION)) {
    if (!this->publish(this->get_action_state_topic(), climate_action_to_mqtt_str(this->device_->action)))
      success = false;
  }

  if (traits.get_supports_fan_modes()) {
    if (this->device_->has_custom_fan_mode()) {
      if (!this->publish(this->get_fan_mode_state_topic(), this->device_->get_custom_fan_mode()))
        success = false;
    } else if (this->device_->fan_mode.has_value()) {
      if (!this->publish(this->get_fan_mode_state_topic(),
                         climate_fan_mode_to_mqtt_str(this->device_->fan_mode.value())))
        success = false;
    } else if (!this->publish(this->get_fan_mode_state_topic(), "")) {
      success = false;
    }
  }

  if (traits.get_supports_swing_modes()) {
    if (!this->publish(this->get_swing_mode_state_topic(), climate_swing_mode_to_mqtt_str(this->device_->swing_mode)))
      success = false;
  }

  return success;
}

}  // namespace esphome::mqtt

#endif
#endif  // USE_MQTT
