#include "mqtt_datetime.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_NUMBER

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.datetime";

using namespace esphome::datetime;

MQTTInputDatetimeComponent::MQTTInputDatetimeComponent(InputDatetime *datetime) : datetime_(datetime) {}

void MQTTInputDatetimeComponent::setup() {
  this->subscribe(this->get_command_topic_(), [this](const std::string &topic, const std::string &state) {
    auto call = this->datetime_->make_call();
    call.set_value(state);
    call.perform();
  });
  this->datetime_->add_on_state_callback([this](std::string state) { this->publish_state(state); });
}

void MQTTInputDatetimeComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT InputDatetime '%s':", this->datetime_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, false)
}

std::string MQTTInputDatetimeComponent::component_type() const { return "datetime"; }
const EntityBase *MQTTInputDatetimeComponent::get_entity() const { return this->datetime_; }

void MQTTInputDatetimeComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  config.command_topic = true;
}
bool MQTTInputDatetimeComponent::send_initial_state() {
  if (this->datetime_->has_state()) {
    return this->publish_state(this->datetime_->state);
  } else {
    return true;
  }
}
bool MQTTInputDatetimeComponent::publish_state(std::string value) {
  // std::string timeString = value.strftime(STRFTIME_FORMAT_FROM_OBJ(this->datetime_, true));
  return this->publish(this->get_state_topic_(), value);
}

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
