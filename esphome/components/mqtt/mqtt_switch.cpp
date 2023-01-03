#include "mqtt_switch.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_SWITCH

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.switch";

using namespace esphome::switch_;

MQTTSwitchComponent::MQTTSwitchComponent(switch_::Switch *a_switch) : switch_(a_switch) {}

void MQTTSwitchComponent::setup() {
  this->subscribe(this->get_command_topic_(), [this](const std::string &topic, const std::string &payload) {
    switch (parse_on_off(payload.c_str())) {
      case PARSE_ON:
        this->switch_->turn_on();
        break;
      case PARSE_OFF:
        this->switch_->turn_off();
        break;
      case PARSE_TOGGLE:
        this->switch_->toggle();
        break;
      case PARSE_NONE:
      default:
        ESP_LOGW(TAG, "'%s': Received unknown status payload: %s", this->friendly_name().c_str(), payload.c_str());
        this->status_momentary_warning("state", 5000);
        break;
    }
  });
  this->switch_->add_on_state_callback(
      [this](bool enabled) { this->defer("send", [this, enabled]() { this->publish_state(enabled); }); });
}
void MQTTSwitchComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Switch '%s': ", this->switch_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, true);
}

std::string MQTTSwitchComponent::component_type() const { return "switch"; }
const EntityBase *MQTTSwitchComponent::get_entity() const { return this->switch_; }
void MQTTSwitchComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  if (this->switch_->assumed_state())
    root[MQTT_OPTIMISTIC] = true;
}
bool MQTTSwitchComponent::send_initial_state() { return this->publish_state(this->switch_->state); }

bool MQTTSwitchComponent::publish_state(bool state) {
  const char *state_s = state ? "ON" : "OFF";
  return this->publish(this->get_state_topic_(), state_s);
}

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
