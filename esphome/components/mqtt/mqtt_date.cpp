#include "mqtt_date.h"

#include <utility>
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_DATETIME_DATE

namespace esphome::mqtt {

ESPHOME_LOG_TAG(TAG, "mqtt.datetime");

using namespace esphome::datetime;

MQTTDateComponent::MQTTDateComponent(DateEntity *date) : date_(date) {}

void MQTTDateComponent::setup() {
  this->subscribe_json(this->command_topic_source_(), [this](const std::string &topic, JsonObject root) {
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

void MQTTDateComponent::dump_config() { LOG_MQTT_COMPONENT(true, true); }

MQTT_COMPONENT_TYPE(MQTTDateComponent, "date")
const EntityBase *MQTTDateComponent::get_entity() const { return this->date_; }

void MQTTDateComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  // Home Assistant sends and expects ISO "YYYY-MM-DD", while state and command payloads here are JSON
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  root[MQTT_VALUE_TEMPLATE] =
      ESPHOME_F("{{ '%04d-%02d-%02d' | format(value_json.year, value_json.month, value_json.day) }}");
  root[MQTT_COMMAND_TEMPLATE] =
      ESPHOME_F("{\"year\":{{ value.year }},\"month\":{{ value.month }},\"day\":{{ value.day }}}");
}
bool MQTTDateComponent::send_initial_state() {
  if (this->date_->has_state()) {
    return this->publish_state(this->date_->year, this->date_->month, this->date_->day);
  } else {
    return true;
  }
}
bool MQTTDateComponent::publish_state(uint16_t year, uint8_t month, uint8_t day) {
  char topic_buf[MQTT_DEFAULT_TOPIC_MAX_LEN];
  return this->publish_json(this->get_state_topic_to_(topic_buf), [year, month, day](JsonObject root) {
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
    root[ESPHOME_F("year")] = year;
    root[ESPHOME_F("month")] = month;
    root[ESPHOME_F("day")] = day;
  });
}

}  // namespace esphome::mqtt

#endif  // USE_DATETIME_DATE
#endif  // USE_MQTT
