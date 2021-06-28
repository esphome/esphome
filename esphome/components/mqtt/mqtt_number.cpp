#include "mqtt_number.h"
#include "esphome/core/log.h"

#ifdef USE_NUMBER

#ifdef USE_DEEP_SLEEP
#include "esphome/components/deep_sleep/deep_sleep_component.h"
#endif

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.number";

using namespace esphome::number;

MQTTNumberComponent::MQTTNumberComponent(Number *number) : MQTTComponent(), number_(number) {}

void MQTTNumberComponent::setup() {
  this->subscribe(this->get_command_topic_(), [this](const std::string &topic, const std::string &state) {
    auto val = parse_float(state.c_str());
    if (!val.has_value()) {
      ESP_LOGW(TAG, "Can't convert '%s' to number!", state.c_str());
      this->publish_state(NAN);
      return;
    }
    this->number_->publish_state(*val);
  });
  this->number_->add_on_state_callback([this](float state) { this->publish_state(state); });
}

void MQTTNumberComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Number '%s':", this->number_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, false)
}

std::string MQTTNumberComponent::component_type() const { return "number"; }

std::string MQTTNumberComponent::friendly_name() const { return this->number_->get_name(); }
void MQTTNumberComponent::send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) {
  if (!this->number_->get_icon().empty())
    root["icon"] = this->number_->get_icon();

  config.command_topic = false;
}
bool MQTTNumberComponent::send_initial_state() {
  if (this->number_->has_state()) {
    return this->publish_state(this->number_->state);
  } else {
    return true;
  }
}
bool MQTTNumberComponent::is_internal() { return this->number_->is_internal(); }
bool MQTTNumberComponent::publish_state(float value) {
  int8_t accuracy = 0;  // TODO
  return this->publish(this->get_state_topic_(), value_accuracy_to_string(value, accuracy));
}
std::string MQTTNumberComponent::unique_id() { return this->number_->unique_id(); }

}  // namespace mqtt
}  // namespace esphome

#endif
