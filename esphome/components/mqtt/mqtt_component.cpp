#include "mqtt_component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/version.h"

namespace esphome {
namespace mqtt {

static const char *TAG = "mqtt.component";

void MQTTComponent::set_retain(bool retain) { this->retain_ = retain; }

std::string MQTTComponent::get_discovery_topic_(const MQTTDiscoveryInfo &discovery_info) const {
  std::string sanitized_name = sanitize_string_allowlist(App.get_name(), HOSTNAME_CHARACTER_ALLOWLIST);
  return discovery_info.prefix + "/" + this->component_type() + "/" + sanitized_name + "/" +
         this->get_default_object_id_() + "/config";
}

std::string MQTTComponent::get_default_topic_for_(const std::string &suffix) const {
  return global_mqtt_client->get_topic_prefix() + "/" + this->component_type() + "/" + this->get_default_object_id_() +
         "/" + suffix;
}

const std::string MQTTComponent::get_state_topic_() const {
  if (this->custom_state_topic_.empty())
    return this->get_default_topic_for_("state");
  return this->custom_state_topic_;
}

const std::string MQTTComponent::get_command_topic_() const {
  if (this->custom_command_topic_.empty())
    return this->get_default_topic_for_("command");
  return this->custom_command_topic_;
}

bool MQTTComponent::publish(const std::string &topic, const std::string &payload) {
  if (topic.empty())
    return false;
  return global_mqtt_client->publish(topic, payload, 0, this->retain_);
}

bool MQTTComponent::publish_json(const std::string &topic, const json::json_build_t &f) {
  if (topic.empty())
    return false;
  return global_mqtt_client->publish_json(topic, f, 0, this->retain_);
}

bool MQTTComponent::send_discovery_() {
  const MQTTDiscoveryInfo &discovery_info = global_mqtt_client->get_discovery_info();

  if (discovery_info.clean) {
    ESP_LOGV(TAG, "'%s': Cleaning discovery...", this->friendly_name().c_str());
    return global_mqtt_client->publish(this->get_discovery_topic_(discovery_info), "", 0, 0, true);
  }

  ESP_LOGV(TAG, "'%s': Sending discovery...", this->friendly_name().c_str());

  return global_mqtt_client->publish_json(
      this->get_discovery_topic_(discovery_info),
      [this](JsonObject &root) {
        SendDiscoveryConfig config;
        config.state_topic = true;
        config.command_topic = true;

        this->send_discovery(root, config);

        std::string name = this->friendly_name();
        root["name"] = name;
        if (config.state_topic)
          root["state_topic"] = this->get_state_topic_();
        if (config.command_topic)
          root["command_topic"] = this->get_command_topic_();

        if (this->availability_ == nullptr) {
          if (!global_mqtt_client->get_availability().topic.empty()) {
            root["availability_topic"] = global_mqtt_client->get_availability().topic;
            if (global_mqtt_client->get_availability().payload_available != "online")
              root["payload_available"] = global_mqtt_client->get_availability().payload_available;
            if (global_mqtt_client->get_availability().payload_not_available != "offline")
              root["payload_not_available"] = global_mqtt_client->get_availability().payload_not_available;
          }
        } else if (!this->availability_->topic.empty()) {
          root["availability_topic"] = this->availability_->topic;
          if (this->availability_->payload_available != "online")
            root["payload_available"] = this->availability_->payload_available;
          if (this->availability_->payload_not_available != "offline")
            root["payload_not_available"] = this->availability_->payload_not_available;
        }

        const std::string &node_name = App.get_name();
        std::string unique_id = this->unique_id();
        if (!unique_id.empty()) {
          root["unique_id"] = unique_id;
        } else {
          // default to almost-unique ID. It's a hack but the only way to get that
          // gorgeous device registry view.
          root["unique_id"] = "ESP" + this->component_type() + this->get_default_object_id_();
        }

        JsonObject &device_info = root.createNestedObject("device");
        device_info["identifiers"] = get_mac_address();
        device_info["name"] = node_name;
        device_info["sw_version"] = "esphome v" ESPHOME_VERSION " " + App.get_compilation_time();
#ifdef ARDUINO_BOARD
        device_info["model"] = ARDUINO_BOARD;
#endif
        device_info["manufacturer"] = "espressif";
      },
      0, discovery_info.retain);
}

bool MQTTComponent::get_retain() const { return this->retain_; }

bool MQTTComponent::is_discovery_enabled() const {
  return this->discovery_enabled_ && global_mqtt_client->is_discovery_enabled();
}

std::string MQTTComponent::get_default_object_id_() const {
  return sanitize_string_allowlist(to_lowercase_underscore(this->friendly_name()), HOSTNAME_CHARACTER_ALLOWLIST);
}

void MQTTComponent::subscribe(const std::string &topic, mqtt_callback_t callback, uint8_t qos) {
  global_mqtt_client->subscribe(topic, std::move(callback), qos);
}

void MQTTComponent::subscribe_json(const std::string &topic, mqtt_json_callback_t callback, uint8_t qos) {
  global_mqtt_client->subscribe_json(topic, std::move(callback), qos);
}

MQTTComponent::MQTTComponent() = default;

float MQTTComponent::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }
void MQTTComponent::disable_discovery() { this->discovery_enabled_ = false; }
void MQTTComponent::set_custom_state_topic(const std::string &custom_state_topic) {
  this->custom_state_topic_ = custom_state_topic;
}
void MQTTComponent::set_custom_command_topic(const std::string &custom_command_topic) {
  this->custom_command_topic_ = custom_command_topic;
}

void MQTTComponent::set_availability(std::string topic, std::string payload_available,
                                     std::string payload_not_available) {
  delete this->availability_;
  this->availability_ = new Availability();
  this->availability_->topic = std::move(topic);
  this->availability_->payload_available = std::move(payload_available);
  this->availability_->payload_not_available = std::move(payload_not_available);
}
void MQTTComponent::disable_availability() { this->set_availability("", "", ""); }
void MQTTComponent::call_setup() {
  if (this->is_internal())
    return;

  this->setup();

  global_mqtt_client->register_mqtt_component(this);

  if (!this->is_connected_())
    return;

  if (this->is_discovery_enabled()) {
    if (!this->send_discovery_()) {
      this->schedule_resend_state();
    }
  }
  if (!this->send_initial_state()) {
    this->schedule_resend_state();
  }
}

void MQTTComponent::call_loop() {
  if (this->is_internal())
    return;

  this->loop();

  if (!this->resend_state_ || !this->is_connected_()) {
    return;
  }

  this->resend_state_ = false;
  if (this->is_discovery_enabled()) {
    if (!this->send_discovery_()) {
      this->schedule_resend_state();
    }
  }
  if (!this->send_initial_state()) {
    this->schedule_resend_state();
  }
}
void MQTTComponent::schedule_resend_state() { this->resend_state_ = true; }
std::string MQTTComponent::unique_id() { return ""; }
bool MQTTComponent::is_connected_() const { return global_mqtt_client->is_connected(); }

}  // namespace mqtt
}  // namespace esphome
