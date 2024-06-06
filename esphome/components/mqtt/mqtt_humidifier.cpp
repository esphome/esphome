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
    // current_humidity_topic
    root[MQTT_CURRENT_HUMIDITY_TOPIC] = this->get_current_humidity_state_topic();
  }
  // mode_command_topic
  root[MQTT_MODE_COMMAND_TOPIC] = this->get_mode_command_topic();
  // mode_state_topic
  root[MQTT_MODE_STATE_TOPIC] = this->get_mode_state_topic();
  // modes
  JsonArray modes = root.createNestedArray(MQTT_MODES);
  // sort array for nice UI in HA
  if (traits.supports_mode(HUMIDIFIER_MODE_AUTO))
    modes.add("auto");
  modes.add("off");
  if (traits.supports_mode(HUMIDIFIER_MODE_HUMIDIFY))
    modes.add("humidify");
  if (traits.supports_mode(HUMIDIFIER_MODE_DEHUMIDIFY))
    modes.add("dehumidify");
  if (traits.supports_mode(HUMIDIFIER_MODE_HUMIDIFY_DEHUMIDIFY))
    modes.add("humidify_dehumidify");

  if (traits.get_supports_two_point_target_humidity()) {
    // humidity_low_command_topic
    root[MQTT_HUMIDITY_LOW_COMMAND_TOPIC] = this->get_target_humidity_low_command_topic();
    // humidity_low_state_topic
    root[MQTT_HUMIDITY_LOW_STATE_TOPIC] = this->get_target_humidity_low_state_topic();
    // humidity_high_command_topic
    root[MQTT_HUMIDITY_HIGH_COMMAND_TOPIC] = this->get_target_humidity_high_command_topic();
    // humidity_high_state_topic
    root[MQTT_HUMIDITY_HIGH_STATE_TOPIC] = this->get_target_humidity_high_state_topic();
  } else {
    // humidity_command_topic
    root[MQTT_HUMIDITY_COMMAND_TOPIC] = this->get_target_humidity_command_topic();
    // humidity_state_topic
    root[MQTT_HUMIDITY_STATE_TOPIC] = this->get_target_humidity_state_topic();
  }

  // min_temp
  root[MQTT_MIN_HUMIDITY] = traits.get_visual_min_humidity();
  // max_temp
  root[MQTT_MAX_HUMIDITY] = traits.get_visual_max_humidity();
  // hum_step
  root["hum_step"] = traits.get_visual_humidity_step();
  // humidity units are always %
  root[MQTT_HUMIDITY_UNIT] = "%";

  if (traits.supports_preset(HUMIDIFIER_PRESET_AWAY)) {
    // away_mode_command_topic
    root[MQTT_AWAY_MODE_COMMAND_TOPIC] = this->get_away_command_topic();
    // away_mode_state_topic
    root[MQTT_AWAY_MODE_STATE_TOPIC] = this->get_away_state_topic();
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

  if (traits.get_supports_two_point_target_humidity()) {
    this->subscribe(this->get_target_humidity_low_command_topic(),
                    [this](const std::string &topic, const std::string &payload) {
                      auto val = parse_number<float>(payload);
                      if (!val.has_value()) {
                        ESP_LOGW(TAG, "Can't convert '%s' to number!", payload.c_str());
                        return;
                      }
                      auto call = this->device_->make_call();
                      call.set_target_humidity_low(*val);
                      call.perform();
                    });
    this->subscribe(this->get_target_humidity_high_command_topic(),
                    [this](const std::string &topic, const std::string &payload) {
                      auto val = parse_number<float>(payload);
                      if (!val.has_value()) {
                        ESP_LOGW(TAG, "Can't convert '%s' to number!", payload.c_str());
                        return;
                      }
                      auto call = this->device_->make_call();
                      call.set_target_humidity_high(*val);
                      call.perform();
                    });
  } else {
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

  if (traits.supports_preset(HUMIDIFIER_PRESET_AWAY)) {
    this->subscribe(this->get_away_command_topic(), [this](const std::string &topic, const std::string &payload) {
      auto onoff = parse_on_off(payload.c_str());
      auto call = this->device_->make_call();
      switch (onoff) {
        case PARSE_ON:
          call.set_preset(HUMIDIFIER_PRESET_AWAY);
          break;
        case PARSE_OFF:
          call.set_preset(HUMIDIFIER_PRESET_HOME);
          break;
        case PARSE_TOGGLE:
          call.set_preset(this->device_->preset == HUMIDIFIER_PRESET_AWAY ? HUMIDIFIER_PRESET_HOME
                                                                          : HUMIDIFIER_PRESET_AWAY);
          break;
        case PARSE_NONE:
        default:
          ESP_LOGW(TAG, "Unknown payload '%s'", payload.c_str());
          return;
      }
      call.perform();
    });
  }

  this->device_->add_on_state_callback([this]() { this->publish_state_(); });
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
    case HUMIDIFIER_MODE_AUTO:
      mode_s = "auto";
      break;
    case HUMIDIFIER_MODE_DEHUMIDIFY:
      mode_s = "dehumidify";
      break;
    case HUMIDIFIER_MODE_HUMIDIFY:
      mode_s = "humidify";
      break;
    case HUMIDIFIER_MODE_HUMIDIFY_DEHUMIDIFY:
      mode_s = "humidify_dehumidify";
      break;
  }
  bool success = true;
  if (!this->publish(this->get_mode_state_topic(), mode_s))
    success = false;
  int8_t accuracy = traits.get_humidity_accuracy_decimals();
  if (traits.get_supports_current_humidity() && !std::isnan(this->device_->current_humidity)) {
    std::string payload = value_accuracy_to_string(this->device_->current_humidity, accuracy);
    if (!this->publish(this->get_current_humidity_state_topic(), payload))
      success = false;
  }
  if (traits.get_supports_two_point_target_humidity()) {
    std::string payload = value_accuracy_to_string(this->device_->target_humidity_low, accuracy);
    if (!this->publish(this->get_target_humidity_low_state_topic(), payload))
      success = false;
    payload = value_accuracy_to_string(this->device_->target_humidity_high, accuracy);
    if (!this->publish(this->get_target_humidity_high_state_topic(), payload))
      success = false;
  } else {
    std::string payload = value_accuracy_to_string(this->device_->target_humidity, accuracy);
    if (!this->publish(this->get_target_humidity_state_topic(), payload))
      success = false;
  }

  if (traits.supports_preset(HUMIDIFIER_PRESET_AWAY)) {
    std::string payload = ONOFF(this->device_->preset == HUMIDIFIER_PRESET_AWAY);
    if (!this->publish(this->get_away_state_topic(), payload))
      success = false;
  }
  if (traits.get_supports_action()) {
    const char *payload = "unknown";
    switch (this->device_->action) {
      case HUMIDIFIER_ACTION_OFF:
        payload = "off";
        break;
      case HUMIDIFIER_ACTION_HUMIDIFYING:
        payload = "humidifying";
        break;
      case HUMIDIFIER_ACTION_IDLE:
        payload = "idle";
        break;
      case HUMIDIFIER_ACTION_DEHUMIDIFYING:
        payload = "dehumidifying";
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
