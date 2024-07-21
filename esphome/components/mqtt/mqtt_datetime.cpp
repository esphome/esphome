#include "mqtt_datetime.h"

#include <utility>
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_DATETIME_DATETIME

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.datetime.datetime";

using namespace esphome::datetime;

MQTTDateTimeComponent::MQTTDateTimeComponent(DateTimeEntity *datetime) : datetime_(datetime) {}

void MQTTDateTimeComponent::setup() {
  this->subscribe_json(this->get_command_topic_(), [this](const std::string &topic, JsonObject root) {
    auto call = this->datetime_->make_call();
    if (root.containsKey("year")) {
      call.set_year(root["year"]);
    }
    if (root.containsKey("month")) {
      call.set_month(root["month"]);
    }
    if (root.containsKey("day")) {
      call.set_day(root["day"]);
    }
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
  this->datetime_->add_on_state_callback([this]() {
    this->publish_state(this->datetime_->year, this->datetime_->month, this->datetime_->day, this->datetime_->hour,
                        this->datetime_->minute, this->datetime_->second);
  });
}

void MQTTDateTimeComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT DateTime '%s':", this->datetime_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, true)
}

std::string MQTTDateTimeComponent::component_type() const { return "datetime"; }
const EntityBase *MQTTDateTimeComponent::get_entity() const { return this->datetime_; }

void MQTTDateTimeComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  // Nothing extra to add here
}
bool MQTTDateTimeComponent::send_initial_state() {
  if (this->datetime_->has_state()) {
    return this->publish_state(this->datetime_->year, this->datetime_->month, this->datetime_->day,
                               this->datetime_->hour, this->datetime_->minute, this->datetime_->second);
  } else {
    return true;
  }
}
bool MQTTDateTimeComponent::publish_state(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
                                          uint8_t second) {
  return this->publish_json(this->get_state_topic_(), [year, month, day, hour, minute, second](JsonObject root) {
    root["year"] = year;
    root["month"] = month;
    root["day"] = day;
    root["hour"] = hour;
    root["minute"] = minute;
    root["second"] = second;
  });
}

}  // namespace mqtt
}  // namespace esphome

#endif  // USE_DATETIME_DATETIME
#endif  // USE_MQTT
