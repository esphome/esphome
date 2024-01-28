#include "mqtt_input_datetime.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_NUMBER

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.input_datetime";

using namespace esphome::input_datetime;

MQTTInputDatetimeComponent::MQTTInputDatetimeComponent(InputDatetime *input_datetime)
    : input_datetime_(input_datetime) {}

void MQTTInputDatetimeComponent::setup() {
  this->subscribe(this->get_command_topic_(), [this](const std::string &topic, const std::string &state) {
    auto call = this->input_datetime_->make_call();
    call.set_value(state);
    call.perform();
  });
  this->input_datetime_->add_on_state_callback([this](std::string state) { this->publish_state(state); });
}

void MQTTInputDatetimeComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT InputDatetime '%s':", this->input_datetime_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, false)
}

std::string MQTTInputDatetimeComponent::component_type() const { return "input_datetime"; }
const EntityBase *MQTTInputDatetimeComponent::get_entity() const { return this->input_datetime_; }

void MQTTInputDatetimeComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  config.command_topic = true;
}
bool MQTTInputDatetimeComponent::send_initial_state() {
  if (this->input_datetime_->has_state()) {
    return this->publish_state(this->input_datetime_->state);
  } else {
    return true;
  }
}
bool MQTTInputDatetimeComponent::publish_state(std::string value) {
  //std::string timeString = value.strftime(STRFTIME_FORMAT_FROM_OBJ(this->input_datetime_, true));
  return this->publish(this->get_state_topic_(), value);
}

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
