#include "mqtt_valve.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_VALVE

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.valve";

using namespace esphome::valve;

MQTTValveComponent::MQTTValveComponent(Valve *valve) : valve_(valve) {}
void MQTTValveComponent::setup() {
  auto traits = this->valve_->get_traits();
  this->valve_->add_on_state_callback([this]() { this->publish_state(); });
  this->subscribe(this->get_command_topic_(), [this](const std::string &topic, const std::string &payload) {
    auto call = this->valve_->make_call();
    call.set_command(payload.c_str());
    call.perform();
  });
  if (traits.get_supports_position()) {
    this->subscribe(this->get_position_command_topic(), [this](const std::string &topic, const std::string &payload) {
      auto value = parse_number<float>(payload);
      if (!value.has_value()) {
        ESP_LOGW(TAG, "Invalid position value: '%s'", payload.c_str());
        return;
      }
      auto call = this->valve_->make_call();
      call.set_position(*value / 100.0f);
      call.perform();
    });
  }
}

void MQTTValveComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT valve '%s':", this->valve_->get_name().c_str());
  auto traits = this->valve_->get_traits();
  bool has_command_topic = traits.get_supports_position();
  LOG_MQTT_COMPONENT(true, has_command_topic)
  if (traits.get_supports_position()) {
    ESP_LOGCONFIG(TAG, "  Position State Topic: '%s'", this->get_position_state_topic().c_str());
    ESP_LOGCONFIG(TAG, "  Position Command Topic: '%s'", this->get_position_command_topic().c_str());
  }
}
void MQTTValveComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  if (!this->valve_->get_device_class().empty())
    root[MQTT_DEVICE_CLASS] = this->valve_->get_device_class();

  auto traits = this->valve_->get_traits();
  if (traits.get_is_assumed_state()) {
    root[MQTT_OPTIMISTIC] = true;
  }
  if (traits.get_supports_position()) {
    root[MQTT_POSITION_TOPIC] = this->get_position_state_topic();
    root[MQTT_SET_POSITION_TOPIC] = this->get_position_command_topic();
  }
}

std::string MQTTValveComponent::component_type() const { return "valve"; }
const EntityBase *MQTTValveComponent::get_entity() const { return this->valve_; }

bool MQTTValveComponent::send_initial_state() { return this->publish_state(); }
bool MQTTValveComponent::publish_state() {
  auto traits = this->valve_->get_traits();
  bool success = true;
  if (traits.get_supports_position()) {
    std::string pos = value_accuracy_to_string(roundf(this->valve_->position * 100), 0);
    if (!this->publish(this->get_position_state_topic(), pos))
      success = false;
  }
  const char *state_s = this->valve_->current_operation == VALVE_OPERATION_OPENING   ? "opening"
                        : this->valve_->current_operation == VALVE_OPERATION_CLOSING ? "closing"
                        : this->valve_->position == VALVE_CLOSED                     ? "closed"
                        : this->valve_->position == VALVE_OPEN                       ? "open"
                        : traits.get_supports_position()                             ? "open"
                                                                                     : "unknown";
  if (!this->publish(this->get_state_topic_(), state_s))
    success = false;
  return success;
}

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
