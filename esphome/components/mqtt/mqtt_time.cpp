#include "mqtt_time.h"

#include <utility>
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_DATETIME_TIME

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.datetime.time";

using namespace esphome::datetime;

MQTTTimeComponent::MQTTTimeComponent(TimeEntity *time) : time_(time) {}

void MQTTTimeComponent::setup() {
  this->subscribe_json(this->get_command_topic_(), [this](const std::string &topic, JsonObject root) {
    auto call = this->time_->make_call();
    if (root.containsKey("hour")) {
      call.set_hour(root["hour"]);
    }
    if (root.containsKey("minute")) {
      call.set_minute(root["minute"]);
    }
    if (root.containsKey("second")) {
      call.set_second(root["second"]);
    }
    call.perform();
  });
  this->time_->add_on_state_callback(
      [this]() { this->publish_state(this->time_->hour, this->time_->minute, this->time_->second); });
}

void MQTTTimeComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Time '%s':", this->time_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, true)
}

std::string MQTTTimeComponent::component_type() const { return "time"; }
const EntityBase *MQTTTimeComponent::get_entity() const { return this->time_; }

void MQTTTimeComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  // Nothing extra to add here
}
bool MQTTTimeComponent::send_initial_state() {
  if (this->time_->has_state()) {
    return this->publish_state(this->time_->hour, this->time_->minute, this->time_->second);
  } else {
    return true;
  }
}
bool MQTTTimeComponent::publish_state(uint8_t hour, uint8_t minute, uint8_t second) {
  return this->publish_json(this->get_state_topic_(), [hour, minute, second](JsonObject root) {
    root["hour"] = hour;
    root["minute"] = minute;
    root["second"] = second;
  });
}

}  // namespace mqtt
}  // namespace esphome

#endif  // USE_DATETIME_TIME
#endif  // USE_MQTT
