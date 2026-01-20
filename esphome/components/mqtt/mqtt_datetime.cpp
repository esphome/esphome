#include "mqtt_datetime.h"

#include <utility>
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_DATETIME_DATETIME

namespace esphome::mqtt {

static const char *const TAG = "mqtt.datetime.datetime";

using namespace esphome::datetime;

MQTTDateTimeComponent::MQTTDateTimeComponent(DateTimeEntity *datetime) : datetime_(datetime) {}

void MQTTDateTimeComponent::setup() {
  this->subscribe_json(this->get_command_topic_(), [this](const std::string &topic, JsonObject root) {
    auto call = this->datetime_->make_call();
    if (root[ESPHOME_F("year")].is<uint16_t>()) {
      call.set_year(root[ESPHOME_F("year")]);
    }
    if (root[ESPHOME_F("month")].is<uint8_t>()) {
      call.set_month(root[ESPHOME_F("month")]);
    }
    if (root[ESPHOME_F("day")].is<uint8_t>()) {
      call.set_day(root[ESPHOME_F("day")]);
    }
    if (root[ESPHOME_F("hour")].is<uint8_t>()) {
      call.set_hour(root[ESPHOME_F("hour")]);
    }
    if (root[ESPHOME_F("minute")].is<uint8_t>()) {
      call.set_minute(root[ESPHOME_F("minute")]);
    }
    if (root[ESPHOME_F("second")].is<uint8_t>()) {
      call.set_second(root[ESPHOME_F("second")]);
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

MQTT_COMPONENT_TYPE(MQTTDateTimeComponent, "datetime")
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
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
    root[ESPHOME_F("year")] = year;
    root[ESPHOME_F("month")] = month;
    root[ESPHOME_F("day")] = day;
    root[ESPHOME_F("hour")] = hour;
    root[ESPHOME_F("minute")] = minute;
    root[ESPHOME_F("second")] = second;
  });
}

}  // namespace esphome::mqtt

#endif  // USE_DATETIME_DATETIME
#endif  // USE_MQTT
