#include "mqtt_cover.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_COVER

namespace esphome::mqtt {

static const char *const TAG = "mqtt.cover";

using namespace esphome::cover;

static ProgmemStr cover_state_to_mqtt_str(CoverOperation operation, float position, bool supports_position) {
  if (operation == COVER_OPERATION_OPENING)
    return ESPHOME_F("opening");
  if (operation == COVER_OPERATION_CLOSING)
    return ESPHOME_F("closing");
  if (position == COVER_CLOSED)
    return ESPHOME_F("closed");
  if (position == COVER_OPEN)
    return ESPHOME_F("open");
  if (supports_position)
    return ESPHOME_F("open");
  return ESPHOME_F("unknown");
}

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
  LOG_MQTT_COMPONENT(true, has_command_topic);
  char topic_buf[MQTT_DEFAULT_TOPIC_MAX_LEN];
#ifdef USE_MQTT_COVER_JSON
  if (this->use_json_format_) {
    ESP_LOGCONFIG(TAG, "  JSON State Payload: YES");
  } else {
#endif
    if (traits.get_supports_position()) {
      ESP_LOGCONFIG(TAG, "  Position State Topic: '%s'", this->get_position_state_topic_to(topic_buf).c_str());
    }
    if (traits.get_supports_tilt()) {
      ESP_LOGCONFIG(TAG, "  Tilt State Topic: '%s'", this->get_tilt_state_topic_to(topic_buf).c_str());
    }
#ifdef USE_MQTT_COVER_JSON
  }
#endif
  if (traits.get_supports_position()) {
    ESP_LOGCONFIG(TAG, "  Position Command Topic: '%s'", this->get_position_command_topic_to(topic_buf).c_str());
  }
  if (traits.get_supports_tilt()) {
    ESP_LOGCONFIG(TAG, "  Tilt Command Topic: '%s'", this->get_tilt_command_topic_to(topic_buf).c_str());
  }
}
void MQTTCoverComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  const auto device_class = this->cover_->get_device_class_ref();
  if (!device_class.empty()) {
    root[MQTT_DEVICE_CLASS] = device_class;
  }
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

  auto traits = this->cover_->get_traits();
  if (traits.get_is_assumed_state()) {
    root[MQTT_OPTIMISTIC] = true;
  }
  char topic_buf[MQTT_DEFAULT_TOPIC_MAX_LEN];
#ifdef USE_MQTT_COVER_JSON
  if (this->use_json_format_) {
    // JSON mode: all state published to state_topic as JSON, use templates to extract
    root[MQTT_VALUE_TEMPLATE] = ESPHOME_F("{{ value_json.state }}");
    if (traits.get_supports_position()) {
      root[MQTT_POSITION_TOPIC] = this->get_state_topic_to_(topic_buf);
      root[MQTT_POSITION_TEMPLATE] = ESPHOME_F("{{ value_json.position }}");
      root[MQTT_SET_POSITION_TOPIC] = this->get_position_command_topic_to(topic_buf);
    }
    if (traits.get_supports_tilt()) {
      root[MQTT_TILT_STATUS_TOPIC] = this->get_state_topic_to_(topic_buf);
      root[MQTT_TILT_STATUS_TEMPLATE] = ESPHOME_F("{{ value_json.tilt }}");
      root[MQTT_TILT_COMMAND_TOPIC] = this->get_tilt_command_topic_to(topic_buf);
    }
  } else
#endif
  {
    // Standard mode: separate topics for position and tilt
    if (traits.get_supports_position()) {
      root[MQTT_POSITION_TOPIC] = this->get_position_state_topic_to(topic_buf);
      root[MQTT_SET_POSITION_TOPIC] = this->get_position_command_topic_to(topic_buf);
    }
    if (traits.get_supports_tilt()) {
      root[MQTT_TILT_STATUS_TOPIC] = this->get_tilt_state_topic_to(topic_buf);
      root[MQTT_TILT_COMMAND_TOPIC] = this->get_tilt_command_topic_to(topic_buf);
    }
  }
  if (traits.get_supports_tilt() && !traits.get_supports_position()) {
    config.command_topic = false;
  }
}

MQTT_COMPONENT_TYPE(MQTTCoverComponent, "cover")
const EntityBase *MQTTCoverComponent::get_entity() const { return this->cover_; }

bool MQTTCoverComponent::send_initial_state() { return this->publish_state(); }
bool MQTTCoverComponent::publish_state() {
  auto traits = this->cover_->get_traits();
  char topic_buf[MQTT_DEFAULT_TOPIC_MAX_LEN];
#ifdef USE_MQTT_COVER_JSON
  if (this->use_json_format_) {
    return this->publish_json(this->get_state_topic_to_(topic_buf), [this, traits](JsonObject root) {
      // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
      root[ESPHOME_F("state")] = cover_state_to_mqtt_str(this->cover_->current_operation, this->cover_->position,
                                                         traits.get_supports_position());
      if (traits.get_supports_position()) {
        root[ESPHOME_F("position")] = static_cast<int>(roundf(this->cover_->position * 100));
      }
      if (traits.get_supports_tilt()) {
        root[ESPHOME_F("tilt")] = static_cast<int>(roundf(this->cover_->tilt * 100));
      }
      // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
    });
  }
#endif
  bool success = true;
  if (traits.get_supports_position()) {
    char pos[VALUE_ACCURACY_MAX_LEN];
    size_t len = value_accuracy_to_buf(pos, roundf(this->cover_->position * 100), 0);
    if (!this->publish(this->get_position_state_topic_to(topic_buf), pos, len))
      success = false;
  }
  if (traits.get_supports_tilt()) {
    char pos[VALUE_ACCURACY_MAX_LEN];
    size_t len = value_accuracy_to_buf(pos, roundf(this->cover_->tilt * 100), 0);
    if (!this->publish(this->get_tilt_state_topic_to(topic_buf), pos, len))
      success = false;
  }
  if (!this->publish(this->get_state_topic_to_(topic_buf),
                     cover_state_to_mqtt_str(this->cover_->current_operation, this->cover_->position,
                                             traits.get_supports_position())))
    success = false;
  return success;
}

}  // namespace esphome::mqtt

#endif
#endif  // USE_MQTT
