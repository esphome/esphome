#include "mqtt_cover.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_COVER

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.cover";

using namespace esphome::cover;

MQTTCoverComponent::MQTTCoverComponent(Cover *cover) : cover_(cover) {}
void MQTTCoverComponent::setup() {
  auto traits = this->cover_->get_traits();
  this->cover_->add_on_state_callback([this]() { this->publish_state(); });
  this->subscribe(this->get_command_topic_(), [this](const std::string &topic, const std::string &payload) {
    auto call = this->cover_->make_call();
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
      auto call = this->cover_->make_call();
      call.set_position(*value / 100.0f);
      call.perform();
    });
  }
  if (traits.get_supports_tilt()) {
    this->subscribe(this->get_tilt_command_topic(), [this](const std::string &topic, const std::string &payload) {
      auto value = parse_number<float>(payload);
      if (!value.has_value()) {
        ESP_LOGW(TAG, "Invalid tilt value: '%s'", payload.c_str());
        return;
      }
      auto call = this->cover_->make_call();
      call.set_tilt(*value / 100.0f);
      call.perform();
    });
  }
}

void MQTTCoverComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT cover '%s':", this->cover_->get_name().c_str());
  auto traits = this->cover_->get_traits();
  bool has_command_topic = traits.get_supports_position() || !traits.get_supports_tilt();
  LOG_MQTT_COMPONENT(true, has_command_topic)
  if (traits.get_supports_position()) {
    ESP_LOGCONFIG(TAG, "  Position State Topic: '%s'", this->get_position_state_topic().c_str());
    ESP_LOGCONFIG(TAG, "  Position Command Topic: '%s'", this->get_position_command_topic().c_str());
  }
  if (traits.get_supports_tilt()) {
    ESP_LOGCONFIG(TAG, "  Tilt State Topic: '%s'", this->get_tilt_state_topic().c_str());
    ESP_LOGCONFIG(TAG, "  Tilt Command Topic: '%s'", this->get_tilt_command_topic().c_str());
  }
}
void MQTTCoverComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  if (!this->cover_->get_device_class().empty())
    root[MQTT_DEVICE_CLASS] = this->cover_->get_device_class();

  auto traits = this->cover_->get_traits();
  if (traits.get_is_assumed_state()) {
    root[MQTT_OPTIMISTIC] = true;
  }
  if (traits.get_supports_position()) {
    root[MQTT_POSITION_TOPIC] = this->get_position_state_topic();
    root[MQTT_SET_POSITION_TOPIC] = this->get_position_command_topic();
  }
  if (traits.get_supports_tilt()) {
    root[MQTT_TILT_STATUS_TOPIC] = this->get_tilt_state_topic();
    root[MQTT_TILT_COMMAND_TOPIC] = this->get_tilt_command_topic();
  }
  if (traits.get_supports_tilt() && !traits.get_supports_position()) {
    config.command_topic = false;
  }
}

std::string MQTTCoverComponent::component_type() const { return "cover"; }
const EntityBase *MQTTCoverComponent::get_entity() const { return this->cover_; }

bool MQTTCoverComponent::send_initial_state() { return this->publish_state(); }
bool MQTTCoverComponent::publish_state() {
  auto traits = this->cover_->get_traits();
  bool success = true;
  if (traits.get_supports_position()) {
    std::string pos = value_accuracy_to_string(roundf(this->cover_->position * 100), 0);
    if (!this->publish(this->get_position_state_topic(), pos))
      success = false;
  }
  if (traits.get_supports_tilt()) {
    std::string pos = value_accuracy_to_string(roundf(this->cover_->tilt * 100), 0);
    if (!this->publish(this->get_tilt_state_topic(), pos))
      success = false;
  }
  const char *state_s = this->cover_->current_operation == COVER_OPERATION_OPENING   ? "opening"
                        : this->cover_->current_operation == COVER_OPERATION_CLOSING ? "closing"
                        : this->cover_->position == COVER_CLOSED                     ? "closed"
                        : this->cover_->position == COVER_OPEN                       ? "open"
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
