#include "mqtt_fan.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_FAN

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.fan";

using namespace esphome::fan;

MQTTFanComponent::MQTTFanComponent(Fan *state) : state_(state) {}

Fan *MQTTFanComponent::get_state() const { return this->state_; }
std::string MQTTFanComponent::component_type() const { return "fan"; }
const EntityBase *MQTTFanComponent::get_entity() const { return this->state_; }

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

  if (this->state_->get_traits().supports_direction()) {
    this->subscribe(this->get_direction_command_topic(), [this](const std::string &topic, const std::string &payload) {
      auto val = parse_on_off(payload.c_str(), "forward", "reverse");
      switch (val) {
        case PARSE_ON:
          ESP_LOGD(TAG, "'%s': Setting direction FORWARD", this->friendly_name().c_str());
          this->state_->make_call().set_direction(fan::FanDirection::FORWARD).perform();
          break;
        case PARSE_OFF:
          ESP_LOGD(TAG, "'%s': Setting direction REVERSE", this->friendly_name().c_str());
          this->state_->make_call().set_direction(fan::FanDirection::REVERSE).perform();
          break;
        case PARSE_TOGGLE:
          this->state_->make_call()
              .set_direction(this->state_->direction == fan::FanDirection::FORWARD ? fan::FanDirection::REVERSE
                                                                                   : fan::FanDirection::FORWARD)
              .perform();
          break;
        case PARSE_NONE:
          ESP_LOGW(TAG, "Unknown direction Payload %s", payload.c_str());
          this->status_momentary_warning("direction", 5000);
          break;
      }
    });
  }

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
    this->subscribe(this->get_speed_level_command_topic(),
                    [this](const std::string &topic, const std::string &payload) {
                      optional<int> speed_level_opt = parse_number<int>(payload);
                      if (speed_level_opt.has_value()) {
                        const int speed_level = speed_level_opt.value();
                        if (speed_level >= 0 && speed_level <= this->state_->get_traits().supported_speed_count()) {
                          ESP_LOGD(TAG, "New speed level %d", speed_level);
                          this->state_->make_call().set_speed(speed_level).perform();
                        } else {
                          ESP_LOGW(TAG, "Invalid speed level %d", speed_level);
                          this->status_momentary_warning("speed", 5000);
                        }
                      } else {
                        ESP_LOGW(TAG, "Invalid speed level %s (int expected)", payload.c_str());
                        this->status_momentary_warning("speed", 5000);
                      }
                    });
  }

  auto f = std::bind(&MQTTFanComponent::publish_state, this);
  this->state_->add_on_state_callback([this, f]() { this->defer("send", f); });
}

void MQTTFanComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Fan '%s': ", this->state_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, true);
  if (this->state_->get_traits().supports_direction()) {
    ESP_LOGCONFIG(TAG, "  Direction State Topic: '%s'", this->get_direction_state_topic().c_str());
    ESP_LOGCONFIG(TAG, "  Direction Command Topic: '%s'", this->get_direction_command_topic().c_str());
  }
  if (this->state_->get_traits().supports_oscillation()) {
    ESP_LOGCONFIG(TAG, "  Oscillation State Topic: '%s'", this->get_oscillation_state_topic().c_str());
    ESP_LOGCONFIG(TAG, "  Oscillation Command Topic: '%s'", this->get_oscillation_command_topic().c_str());
  }
  if (this->state_->get_traits().supports_speed()) {
    ESP_LOGCONFIG(TAG, "  Speed Level State Topic: '%s'", this->get_speed_level_state_topic().c_str());
    ESP_LOGCONFIG(TAG, "  Speed Level Command Topic: '%s'", this->get_speed_level_command_topic().c_str());
  }
}

bool MQTTFanComponent::send_initial_state() { return this->publish_state(); }

void MQTTFanComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  if (this->state_->get_traits().supports_direction()) {
    root[MQTT_DIRECTION_COMMAND_TOPIC] = this->get_direction_command_topic();
    root[MQTT_DIRECTION_STATE_TOPIC] = this->get_direction_state_topic();
  }
  if (this->state_->get_traits().supports_oscillation()) {
    root[MQTT_OSCILLATION_COMMAND_TOPIC] = this->get_oscillation_command_topic();
    root[MQTT_OSCILLATION_STATE_TOPIC] = this->get_oscillation_state_topic();
  }
  if (this->state_->get_traits().supports_speed()) {
    root[MQTT_PERCENTAGE_COMMAND_TOPIC] = this->get_speed_level_command_topic();
    root[MQTT_PERCENTAGE_STATE_TOPIC] = this->get_speed_level_state_topic();
    root[MQTT_SPEED_RANGE_MAX] = this->state_->get_traits().supported_speed_count();
  }
}
bool MQTTFanComponent::publish_state() {
  const char *state_s = this->state_->state ? "ON" : "OFF";
  ESP_LOGD(TAG, "'%s' Sending state %s.", this->state_->get_name().c_str(), state_s);
  this->publish(this->get_state_topic_(), state_s);
  bool failed = false;
  if (this->state_->get_traits().supports_direction()) {
    bool success = this->publish(this->get_direction_state_topic(),
                                 this->state_->direction == fan::FanDirection::FORWARD ? "forward" : "reverse");
    failed = failed || !success;
  }
  if (this->state_->get_traits().supports_oscillation()) {
    bool success = this->publish(this->get_oscillation_state_topic(),
                                 this->state_->oscillating ? "oscillate_on" : "oscillate_off");
    failed = failed || !success;
  }
  auto traits = this->state_->get_traits();
  if (traits.supports_speed()) {
    std::string payload = to_string(this->state_->speed);
    bool success = this->publish(this->get_speed_level_state_topic(), payload);
    failed = failed || !success;
  }
  return !failed;
}

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
