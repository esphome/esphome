#include "mqtt_fan.h"
#include "esphome/core/log.h"

#ifdef USE_FAN
#include "esphome/components/fan/fan_helpers.h"

namespace esphome {
namespace mqtt {

static const char *TAG = "mqtt.fan";

using namespace esphome::fan;

MQTTFanComponent::MQTTFanComponent(FanState *state) : MQTTComponent(), state_(state) {}

FanState *MQTTFanComponent::get_state() const { return this->state_; }
std::string MQTTFanComponent::component_type() const { return "fan"; }
void MQTTFanComponent::setup() {
  this->subscribe(this->get_command_topic_(), [this](const std::string &topic, const std::string &payload) {
    auto val = parse_on_off(payload.c_str());
    switch (val) {
      case PARSE_ON:
        ESP_LOGD(TAG, "'%s' Turning Fan ON.", this->friendly_name().c_str());
        this->state_->turn_on().perform();
        break;
      case PARSE_OFF:
        ESP_LOGD(TAG, "'%s' Turning Fan OFF.", this->friendly_name().c_str());
        this->state_->turn_off().perform();
        break;
      case PARSE_TOGGLE:
        ESP_LOGD(TAG, "'%s' Toggling Fan.", this->friendly_name().c_str());
        this->state_->toggle().perform();
        break;
      case PARSE_NONE:
      default:
        ESP_LOGW(TAG, "Unknown state payload %s", payload.c_str());
        this->status_momentary_warning("state", 5000);
        break;
    }
  });

  if (this->state_->get_traits().supports_oscillation()) {
    this->subscribe(this->get_oscillation_command_topic(),
                    [this](const std::string &topic, const std::string &payload) {
                      auto val = parse_on_off(payload.c_str(), "oscillate_on", "oscillate_off");
                      switch (val) {
                        case PARSE_ON:
                          ESP_LOGD(TAG, "'%s': Setting oscillating ON", this->friendly_name().c_str());
                          this->state_->make_call().set_oscillating(true).perform();
                          break;
                        case PARSE_OFF:
                          ESP_LOGD(TAG, "'%s': Setting oscillating OFF", this->friendly_name().c_str());
                          this->state_->make_call().set_oscillating(false).perform();
                          break;
                        case PARSE_TOGGLE:
                          this->state_->make_call().set_oscillating(!this->state_->oscillating).perform();
                          break;
                        case PARSE_NONE:
                          ESP_LOGW(TAG, "Unknown Oscillation Payload %s", payload.c_str());
                          this->status_momentary_warning("oscillation", 5000);
                          break;
                      }
                    });
  }

  if (this->state_->get_traits().supports_speed()) {
    this->subscribe(this->get_speed_command_topic(), [this](const std::string &topic, const std::string &payload) {
      this->state_->make_call().set_speed(payload.c_str()).perform();
    });
  }

  auto f = std::bind(&MQTTFanComponent::publish_state, this);
  this->state_->add_on_state_callback([this, f]() { this->defer("send", f); });
}
bool MQTTFanComponent::send_initial_state() { return this->publish_state(); }
std::string MQTTFanComponent::friendly_name() const { return this->state_->get_name(); }
void MQTTFanComponent::send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) {
  if (this->state_->get_traits().supports_oscillation()) {
    root["oscillation_command_topic"] = this->get_oscillation_command_topic();
    root["oscillation_state_topic"] = this->get_oscillation_state_topic();
  }
  if (this->state_->get_traits().supports_speed()) {
    root["speed_command_topic"] = this->get_speed_command_topic();
    root["speed_state_topic"] = this->get_speed_state_topic();
  }
}
bool MQTTFanComponent::is_internal() { return this->state_->is_internal(); }
bool MQTTFanComponent::publish_state() {
  const char *state_s = this->state_->state ? "ON" : "OFF";
  ESP_LOGD(TAG, "'%s' Sending state %s.", this->state_->get_name().c_str(), state_s);
  this->publish(this->get_state_topic_(), state_s);
  bool failed = false;
  if (this->state_->get_traits().supports_oscillation()) {
    bool success = this->publish(this->get_oscillation_state_topic(),
                                 this->state_->oscillating ? "oscillate_on" : "oscillate_off");
    failed = failed || !success;
  }
  auto traits = this->state_->get_traits();
  if (traits.supports_speed()) {
    const char *payload;
    switch (fan::speed_level_to_enum(this->state_->speed, traits.supported_speed_count())) {
      case FAN_SPEED_LOW: {
        payload = "low";
        break;
      }
      case FAN_SPEED_MEDIUM: {
        payload = "medium";
        break;
      }
      default:
      case FAN_SPEED_HIGH: {
        payload = "high";
        break;
      }
    }
    bool success = this->publish(this->get_speed_state_topic(), payload);
    failed = failed || !success;
  }

  return !failed;
}

}  // namespace mqtt
}  // namespace esphome

#endif
