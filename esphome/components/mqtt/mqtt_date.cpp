#include "mqtt_date.h"

#include <utility>
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_DATETIME_DATE

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.datetime";

using namespace esphome::datetime;

MQTTDateComponent::MQTTDateComponent(DateEntity *date) : date_(date) {}

void MQTTDateComponent::setup() {
  this->subscribe_json(this->get_command_topic_(), [this](const std::string &topic, JsonObject root) {
    auto call = this->date_->make_call();
    if (root.containsKey("year")) {
      call.set_year(root["year"]);
    }
    if (root.containsKey("month")) {
      call.set_month(root["month"]);
    }
    if (root.containsKey("day")) {
      call.set_day(root["day"]);
    }
    call.perform();
  });
  this->date_->add_on_state_callback(
      [this]() { this->publish_state(this->date_->year, this->date_->month, this->date_->day); });
}

void MQTTDateComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Date '%s':", this->date_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, true)
}

std::string MQTTDateComponent::component_type() const { return "date"; }
const EntityBase *MQTTDateComponent::get_entity() const { return this->date_; }

void MQTTDateComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  // Nothing extra to add here
}
bool MQTTDateComponent::send_initial_state() {
  if (this->date_->has_state()) {
    return this->publish_state(this->date_->year, this->date_->month, this->date_->day);
  } else {
    return true;
  }
}
bool MQTTDateComponent::publish_state(uint16_t year, uint8_t month, uint8_t day) {
  return this->publish_json(this->get_state_topic_(), [year, month, day](JsonObject root) {
    root["year"] = year;
    root["month"] = month;
    root["day"] = day;
  });
}

}  // namespace mqtt
}  // namespace esphome

#endif  // USE_DATETIME_DATE
#endif  // USE_MQTT
