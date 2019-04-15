#include "mqtt_cover.h"
#include "esphome/core/log.h"

#ifdef USE_COVER

namespace esphome {
namespace mqtt {

static const char *TAG = "mqtt.cover";

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
      auto value = parse_float(payload);
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
      auto value = parse_float(payload);
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
  // no state topic for position
  bool state_topic = !traits.get_supports_position();
  LOG_MQTT_COMPONENT(state_topic, true)
  if (!state_topic) {
    ESP_LOGCONFIG(TAG, "  Position State Topic: '%s'", this->get_position_state_topic().c_str());
    ESP_LOGCONFIG(TAG, "  Position Command Topic: '%s'", this->get_position_command_topic().c_str());
  }
  if (traits.get_supports_tilt()) {
    ESP_LOGCONFIG(TAG, "  Tilt State Topic: '%s'", this->get_tilt_state_topic().c_str());
    ESP_LOGCONFIG(TAG, "  Tilt Command Topic: '%s'", this->get_tilt_command_topic().c_str());
  }
}
void MQTTCoverComponent::send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) {
  auto traits = this->cover_->get_traits();
  if (traits.get_is_assumed_state()) {
    root["optimistic"] = true;
  }
  if (traits.get_supports_position()) {
    root["position_topic"] = this->get_position_state_topic();
    root["set_position_topic"] = this->get_position_command_topic();
  }
  if (traits.get_supports_tilt()) {
    root["tilt_status_topic"] = this->get_tilt_state_topic();
    root["tilt_command_topic"] = this->get_tilt_command_topic();
  }
}

std::string MQTTCoverComponent::component_type() const { return "cover"; }
std::string MQTTCoverComponent::friendly_name() const { return this->cover_->get_name(); }
bool MQTTCoverComponent::send_initial_state() { return this->publish_state(); }
bool MQTTCoverComponent::is_internal() { return this->cover_->is_internal(); }
bool MQTTCoverComponent::publish_state() {
  auto traits = this->cover_->get_traits();
  bool success = true;
  if (!traits.get_supports_position()) {
    const char *state_s = "unknown";
    if (this->cover_->position == COVER_OPEN) {
      state_s = "open";
    } else if (this->cover_->position == COVER_CLOSED) {
      state_s = "closed";
    }

    if (!this->publish(this->get_state_topic_(), state_s))
      success = false;
  } else {
    std::string pos = value_accuracy_to_string(roundf(this->cover_->position * 100), 0);
    if (!this->publish(this->get_position_state_topic(), pos))
      success = false;
  }
  if (traits.get_supports_tilt()) {
    std::string pos = value_accuracy_to_string(roundf(this->cover_->tilt * 100), 0);
    if (!this->publish(this->get_tilt_state_topic(), pos))
      success = false;
  }
  return success;
}

}  // namespace mqtt
}  // namespace esphome

#endif
