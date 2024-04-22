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
  if (traits.supports_mode(HUMIDIFIER_MODE_NORMAL))
    modes.add("normal");
  modes.add("off");
  if (traits.supports_mode(HUMIDIFIER_MODE_ECO))
    modes.add("eco");
  if (traits.supports_mode(HUMIDIFIER_MODE_AWAY))
    modes.add("away");
  if (traits.supports_mode(HUMIDIFIER_MODE_BOOST))
    modes.add("boost");
  if (traits.supports_mode(HUMIDIFIER_MODE_COMFORT))
    modes.add("comfort");
  if (traits.supports_mode(HUMIDIFIER_MODE_HOME))
    modes.add("home");
  if (traits.supports_mode(HUMIDIFIER_MODE_SLEEP))
    modes.add("sleep");
  if (traits.supports_mode(HUMIDIFIER_MODE_AUTO))
    modes.add("auto");
  if (traits.supports_mode(HUMIDIFIER_MODE_BOOST))
    modes.add("baby");

  if (traits.get_supports_target_humidity()) {
    // humidity_command_topic
    root[MQTT_TARGET_HUMIDITY_COMMAND_TOPIC] = this->get_target_humidity_command_topic();
    // humidity_state_topic
    root[MQTT_TARGET_HUMIDITY_STATE_TOPIC] = this->get_target_humidity_state_topic();
  }

  // min_humidity
  root[MQTT_MIN_HUMIDITY] = traits.get_visual_min_humidity();
  // max_humidity
  root[MQTT_MAX_HUMIDITY] = traits.get_visual_max_humidity();
  // humidity_step
  root["humi_step"] = traits.get_visual_target_humidity_step();
  // // humidity units are always coerced to percentage internally
  // root[MQTT_HUMIDITY_UNIT] = "%";

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
    case HUMIDIFIER_MODE_NORMAL:
      mode_s = "normal";
      break;
    case HUMIDIFIER_MODE_ECO:
      mode_s = "eco";
      break;
    case HUMIDIFIER_MODE_AWAY:
      mode_s = "away";
      break;
    case HUMIDIFIER_MODE_BOOST:
      mode_s = "boost";
      break;
    case HUMIDIFIER_MODE_COMFORT:
      mode_s = "comfort";
      break;
    case HUMIDIFIER_MODE_HOME:
      mode_s = "home";
      break;
    case HUMIDIFIER_MODE_SLEEP:
      mode_s = "sleep";
      break;
    case HUMIDIFIER_MODE_AUTO:
      mode_s = "auto";
      break;
    case HUMIDIFIER_MODE_BABY:
      mode_s = "baby";
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

  if (traits.get_supports_action()) {
    const char *payload = "unknown";
    switch (this->device_->action) {
      case HUMIDIFIER_ACTION_OFF:
        payload = "off";
        break;
      case HUMIDIFIER_ACTION_NORMAL:
        payload = "normal";
        break;
      case HUMIDIFIER_ACTION_ECO:
        payload = "eco";
        break;
      case HUMIDIFIER_ACTION_AWAY:
        payload = "away";
        break;
      case HUMIDIFIER_ACTION_BOOST:
        payload = "boost";
        break;
      case HUMIDIFIER_ACTION_COMFORT:
        payload = "comfort";
        break;
      case HUMIDIFIER_ACTION_HOME:
        payload = "home";
        break;
      case HUMIDIFIER_ACTION_SLEEP:
        payload = "sleep";
        break;
      case HUMIDIFIER_ACTION_AUTO:
        payload = "auto";
        break;
      case HUMIDIFIER_ACTION_BABY:
        payload = "baby";
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
