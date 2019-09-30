#include "mqtt_climate.h"
#include "esphome/core/log.h"

#ifdef USE_CLIMATE

namespace esphome {
namespace mqtt {

static const char *TAG = "mqtt.climate";

using namespace esphome::climate;

void MQTTClimateComponent::send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) {
  auto traits = this->device_->get_traits();
  // current_temperature_topic
  if (traits.get_supports_current_temperature()) {
    root["current_temperature_topic"] = this->get_current_temperature_state_topic();
  }
  // mode_command_topic
  root["mode_command_topic"] = this->get_mode_command_topic();
  // mode_state_topic
  root["mode_state_topic"] = this->get_mode_state_topic();
  // modes
  JsonArray &modes = root.createNestedArray("modes");
  // sort array for nice UI in HA
  if (traits.supports_mode(CLIMATE_MODE_AUTO))
    modes.add("auto");
  modes.add("off");
  if (traits.supports_mode(CLIMATE_MODE_COOL))
    modes.add("cool");
  if (traits.supports_mode(CLIMATE_MODE_HEAT))
    modes.add("heat");

  if (traits.get_supports_two_point_target_temperature()) {
    // temperature_low_command_topic
    root["temperature_low_command_topic"] = this->get_target_temperature_low_command_topic();
    // temperature_low_state_topic
    root["temperature_low_state_topic"] = this->get_target_temperature_low_state_topic();
    // temperature_high_command_topic
    root["temperature_high_command_topic"] = this->get_target_temperature_high_command_topic();
    // temperature_high_state_topic
    root["temperature_high_state_topic"] = this->get_target_temperature_high_state_topic();
  } else {
    // temperature_command_topic
    root["temperature_command_topic"] = this->get_target_temperature_command_topic();
    // temperature_state_topic
    root["temperature_state_topic"] = this->get_target_temperature_state_topic();
  }

  // min_temp
  root["min_temp"] = traits.get_visual_min_temperature();
  // max_temp
  root["max_temp"] = traits.get_visual_max_temperature();
  // temp_step
  root["temp_step"] = traits.get_visual_temperature_step();

  if (traits.get_supports_away()) {
    // away_mode_command_topic
    root["away_mode_command_topic"] = this->get_away_command_topic();
    // away_mode_state_topic
    root["away_mode_state_topic"] = this->get_away_state_topic();
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
                      auto val = parse_float(payload);
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
                      auto val = parse_float(payload);
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
                      auto val = parse_float(payload);
                      if (!val.has_value()) {
                        ESP_LOGW(TAG, "Can't convert '%s' to number!", payload.c_str());
                        return;
                      }
                      auto call = this->device_->make_call();
                      call.set_target_temperature(*val);
                      call.perform();
                    });
  }

  if (traits.get_supports_away()) {
    this->subscribe(this->get_away_command_topic(), [this](const std::string &topic, const std::string &payload) {
      auto onoff = parse_on_off(payload.c_str());
      auto call = this->device_->make_call();
      switch (onoff) {
        case PARSE_ON:
          call.set_away(true);
          break;
        case PARSE_OFF:
          call.set_away(false);
          break;
        case PARSE_TOGGLE:
          call.set_away(!this->device_->away);
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
MQTTClimateComponent::MQTTClimateComponent(Climate *device) : device_(device) {}
bool MQTTClimateComponent::send_initial_state() { return this->publish_state_(); }
bool MQTTClimateComponent::is_internal() { return this->device_->is_internal(); }
std::string MQTTClimateComponent::component_type() const { return "climate"; }
std::string MQTTClimateComponent::friendly_name() const { return this->device_->get_name(); }
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
  }
  bool success = true;
  if (!this->publish(this->get_mode_state_topic(), mode_s))
    success = false;
  int8_t accuracy = traits.get_temperature_accuracy_decimals();
  if (traits.get_supports_current_temperature() && !isnan(this->device_->current_temperature)) {
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

  if (traits.get_supports_away()) {
    std::string payload = ONOFF(this->device_->away);
    if (!this->publish(this->get_away_state_topic(), payload))
      success = false;
  }

  return success;
}

}  // namespace mqtt
}  // namespace esphome

#endif
