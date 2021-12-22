#include "mqtt_number.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_NUMBER

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.number";

using namespace esphome::number;

MQTTNumberComponent::MQTTNumberComponent(Number *number) : MQTTComponent(), number_(number) {}

void MQTTNumberComponent::setup() {
  this->subscribe(this->get_command_topic_(), [this](const std::string &topic, const std::string &state) {
    auto val = parse_number<float>(state);
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
const EntityBase *MQTTNumberComponent::get_entity() const { return this->number_; }

void MQTTNumberComponent::send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) {
  const auto &traits = number_->traits;
  // https://www.home-assistant.io/integrations/number.mqtt/
  root[MQTT_MIN] = traits.get_min_value();
  root[MQTT_MAX] = traits.get_max_value();
  root[MQTT_STEP] = traits.get_step();
  if (!this->number_->traits.get_unit_of_measurement().empty())
    root[MQTT_UNIT_OF_MEASUREMENT] = this->number_->traits.get_unit_of_measurement();
  switch (this->number_->traits.get_mode()) {
    case NUMBER_MODE_AUTO:
      break;
    case NUMBER_MODE_BOX:
      root[MQTT_MODE] = "box";
      break;
    case NUMBER_MODE_SLIDER:
      root[MQTT_MODE] = "slider";
      break;
  }

  config.command_topic = true;
}
bool MQTTNumberComponent::send_initial_state() {
  if (this->number_->has_state()) {
    return this->publish_state(this->number_->state);
  } else {
    return true;
  }
}
bool MQTTNumberComponent::publish_state(float value) {
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "%f", value);
  return this->publish(this->get_state_topic_(), buffer);
}

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
