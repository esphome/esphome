#include "mqtt_number.h"
#include "esphome/core/log.h"

#ifdef USE_NUMBER

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.number";

using namespace esphome::number;

MQTTNumberComponent::MQTTNumberComponent(Number *number) : MQTTComponent(), number_(number) {}

void MQTTNumberComponent::setup() {
  this->subscribe(this->get_command_topic_(), [this](const std::string &topic, const std::string &state) {
    auto val = parse_float(state);
    if (!val.has_value()) {
      ESP_LOGW(TAG, "Can't convert '%s' to number!", state.c_str());
      return;
    }
    auto call = this->number_->make_call();
    call.set_value(*val);
    call.perform();
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
  const auto &traits = number_->traits;
  // https://www.home-assistant.io/integrations/number.mqtt/
  if (!traits.get_icon().empty())
    root["icon"] = traits.get_icon();
  root["min_value"] = traits.get_min_value();
  root["max_value"] = traits.get_max_value();
  root["step"] = traits.get_step();

  config.command_topic = true;
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
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "%f", value);
  return this->publish(this->get_state_topic_(), buffer);
}

}  // namespace mqtt
}  // namespace esphome

#endif
