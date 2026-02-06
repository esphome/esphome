#include "mqtt_text.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_TEXT

namespace esphome::mqtt {

static const char *const TAG = "mqtt.text";

using namespace esphome::text;

// Text mode MQTT strings indexed by TextMode enum (0-1): TEXT, PASSWORD
PROGMEM_STRING_TABLE(TextMqttModeStrings, "text", "password");

MQTTTextComponent::MQTTTextComponent(Text *text) : text_(text) {}

void MQTTTextComponent::setup() {
  this->subscribe(this->get_command_topic_(), [this](const std::string &topic, const std::string &state) {
    auto call = this->text_->make_call();
    call.set_value(state);
    call.perform();
  });

  this->text_->add_on_state_callback([this](const std::string &state) { this->publish_state(state); });
}

void MQTTTextComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT text '%s':", this->text_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, true);
}

MQTT_COMPONENT_TYPE(MQTTTextComponent, "text")
const EntityBase *MQTTTextComponent::get_entity() const { return this->text_; }

void MQTTTextComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  root[MQTT_MODE] = TextMqttModeStrings::get_progmem_str(static_cast<uint8_t>(this->text_->traits.get_mode()),
                                                         static_cast<uint8_t>(TEXT_MODE_TEXT));

  config.command_topic = true;
}
bool MQTTTextComponent::send_initial_state() {
  if (this->text_->has_state()) {
    return this->publish_state(this->text_->state);
  } else {
    return true;
  }
}
bool MQTTTextComponent::publish_state(const std::string &value) {
  char topic_buf[MQTT_DEFAULT_TOPIC_MAX_LEN];
  return this->publish(this->get_state_topic_to_(topic_buf), value.data(), value.size());
}

}  // namespace esphome::mqtt

#endif
#endif  // USE_MQTT
