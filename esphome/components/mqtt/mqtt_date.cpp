#include "mqtt_date.h"

#include <utility>
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_DATETIME_DATE

namespace esphome::mqtt {

static const char *const TAG = "mqtt.datetime";

using namespace esphome::datetime;

MQTTDateComponent::MQTTDateComponent(DateEntity *date) : date_(date) {}

void MQTTDateComponent::setup() {
  this->subscribe_json(this->get_command_topic_(), [this](const std::string &topic, JsonObject root) {
    auto call = this->date_->make_call();
    if (root[ESPHOME_F("year")].is<uint16_t>()) {
      call.set_year(root[ESPHOME_F("year")]);
    }
    if (root[ESPHOME_F("month")].is<uint8_t>()) {
      call.set_month(root[ESPHOME_F("month")]);
    }
    if (root[ESPHOME_F("day")].is<uint8_t>()) {
      call.set_day(root[ESPHOME_F("day")]);
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

MQTT_COMPONENT_TYPE(MQTTDateComponent, "date")
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
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
    root[ESPHOME_F("year")] = year;
    root[ESPHOME_F("month")] = month;
    root[ESPHOME_F("day")] = day;
  });
}

}  // namespace esphome::mqtt

#endif  // USE_DATETIME_DATE
#endif  // USE_MQTT
