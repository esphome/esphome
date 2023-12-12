#include "mqtt_component.h"

#ifdef USE_MQTT

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"

#include "mqtt_const.h"

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.component";

void MQTTComponent::set_retain(bool retain) { this->retain_ = retain; }

std::string MQTTComponent::get_discovery_topic_(const MQTTDiscoveryInfo &discovery_info) const {
  std::string sanitized_name = str_sanitize(App.get_name());
  return discovery_info.prefix + "/" + this->component_type() + "/" + sanitized_name + "/" +
         this->get_default_object_id_() + "/config";
}

std::string MQTTComponent::get_default_topic_for_(const std::string &suffix) const {
  const std::string &topic_prefix = global_mqtt_client->get_topic_prefix();
  if (topic_prefix.empty()) {
    // If the topic_prefix is null, the default topic should be null
    return "";
  }

  return topic_prefix + "/" + this->component_type() + "/" + this->get_default_object_id_() + "/" + suffix;
}

std::string MQTTComponent::get_state_topic_() const {
  if (this->has_custom_state_topic_)
    return this->custom_state_topic_;
  return this->get_default_topic_for_("state");
}

std::string MQTTComponent::get_command_topic_() const {
  if (this->has_custom_command_topic_)
    return this->custom_command_topic_;
  return this->get_default_topic_for_("command");
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
      [this](JsonObject root) {
        SendDiscoveryConfig config;
        config.state_topic = true;
        config.command_topic = true;

        this->send_discovery(root, config);

        // Fields from EntityBase
        if (this->get_entity()->has_own_name()) {
          root[MQTT_NAME] = this->friendly_name();
        } else {
          root[MQTT_NAME] = "";
        }
        if (this->is_disabled_by_default())
          root[MQTT_ENABLED_BY_DEFAULT] = false;
        if (!this->get_icon().empty())
          root[MQTT_ICON] = this->get_icon();

        switch (this->get_entity()->get_entity_category()) {
          case ENTITY_CATEGORY_NONE:
            break;
          case ENTITY_CATEGORY_CONFIG:
            root[MQTT_ENTITY_CATEGORY] = "config";
            break;
          case ENTITY_CATEGORY_DIAGNOSTIC:
            root[MQTT_ENTITY_CATEGORY] = "diagnostic";
            break;
        }

        if (config.state_topic)
          root[MQTT_STATE_TOPIC] = this->get_state_topic_();
        if (config.command_topic)
          root[MQTT_COMMAND_TOPIC] = this->get_command_topic_();
        if (this->command_retain_)
          root[MQTT_COMMAND_RETAIN] = true;

        if (this->availability_ == nullptr) {
          if (!global_mqtt_client->get_availability().topic.empty()) {
            root[MQTT_AVAILABILITY_TOPIC] = global_mqtt_client->get_availability().topic;
            if (global_mqtt_client->get_availability().payload_available != "online")
              root[MQTT_PAYLOAD_AVAILABLE] = global_mqtt_client->get_availability().payload_available;
            if (global_mqtt_client->get_availability().payload_not_available != "offline")
              root[MQTT_PAYLOAD_NOT_AVAILABLE] = global_mqtt_client->get_availability().payload_not_available;
          }
        } else if (!this->availability_->topic.empty()) {
          root[MQTT_AVAILABILITY_TOPIC] = this->availability_->topic;
          if (this->availability_->payload_available != "online")
            root[MQTT_PAYLOAD_AVAILABLE] = this->availability_->payload_available;
          if (this->availability_->payload_not_available != "offline")
            root[MQTT_PAYLOAD_NOT_AVAILABLE] = this->availability_->payload_not_available;
        }

        std::string unique_id = this->unique_id();
        const MQTTDiscoveryInfo &discovery_info = global_mqtt_client->get_discovery_info();
        if (!unique_id.empty()) {
          root[MQTT_UNIQUE_ID] = unique_id;
        } else {
          if (discovery_info.unique_id_generator == MQTT_MAC_ADDRESS_UNIQUE_ID_GENERATOR) {
            char friendly_name_hash[9];
            sprintf(friendly_name_hash, "%08" PRIx32, fnv1_hash(this->friendly_name()));
            friendly_name_hash[8] = 0;  // ensure the hash-string ends with null
            root[MQTT_UNIQUE_ID] = get_mac_address() + "-" + this->component_type() + "-" + friendly_name_hash;
          } else {
            // default to almost-unique ID. It's a hack but the only way to get that
            // gorgeous device registry view.
            root[MQTT_UNIQUE_ID] = "ESP" + this->component_type() + this->get_default_object_id_();
          }
        }

        const std::string &node_name = App.get_name();
        if (discovery_info.object_id_generator == MQTT_DEVICE_NAME_OBJECT_ID_GENERATOR)
          root[MQTT_OBJECT_ID] = node_name + "_" + this->get_default_object_id_();

        std::string node_friendly_name = App.get_friendly_name();
        if (node_friendly_name.empty()) {
          node_friendly_name = node_name;
        }
        const std::string &node_area = App.get_area();

        JsonObject device_info = root.createNestedObject(MQTT_DEVICE);
        device_info[MQTT_DEVICE_IDENTIFIERS] = get_mac_address();
        device_info[MQTT_DEVICE_NAME] = node_friendly_name;
        device_info[MQTT_DEVICE_SW_VERSION] = "esphome v" ESPHOME_VERSION " " + App.get_compilation_time();
        device_info[MQTT_DEVICE_MODEL] = ESPHOME_BOARD;
        device_info[MQTT_DEVICE_MANUFACTURER] = "espressif";
        device_info[MQTT_DEVICE_SUGGESTED_AREA] = node_area;
      },
      0, discovery_info.retain);
}

bool MQTTComponent::get_retain() const { return this->retain_; }

bool MQTTComponent::is_discovery_enabled() const {
  return this->discovery_enabled_ && global_mqtt_client->is_discovery_enabled();
}

std::string MQTTComponent::get_default_object_id_() const {
  return str_sanitize(str_snake_case(this->friendly_name()));
}

void MQTTComponent::subscribe(const std::string &topic, mqtt_callback_t callback, uint8_t qos) {
  global_mqtt_client->subscribe(topic, std::move(callback), qos);
}

void MQTTComponent::subscribe_json(const std::string &topic, const mqtt_json_callback_t &callback, uint8_t qos) {
  global_mqtt_client->subscribe_json(topic, callback, qos);
}

MQTTComponent::MQTTComponent() = default;

float MQTTComponent::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }
void MQTTComponent::disable_discovery() { this->discovery_enabled_ = false; }
void MQTTComponent::set_custom_state_topic(const std::string &custom_state_topic) {
  this->custom_state_topic_ = custom_state_topic;
  this->has_custom_state_topic_ = true;
}
void MQTTComponent::set_custom_command_topic(const std::string &custom_command_topic) {
  this->custom_command_topic_ = custom_command_topic;
  this->has_custom_command_topic_ = true;
}
void MQTTComponent::set_command_retain(bool command_retain) { this->command_retain_ = command_retain; }

void MQTTComponent::set_availability(std::string topic, std::string payload_available,
                                     std::string payload_not_available) {
  this->availability_ = make_unique<Availability>();
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
void MQTTComponent::call_dump_config() {
  if (this->is_internal())
    return;

  this->dump_config();
}
void MQTTComponent::schedule_resend_state() { this->resend_state_ = true; }
std::string MQTTComponent::unique_id() { return ""; }
bool MQTTComponent::is_connected_() const { return global_mqtt_client->is_connected(); }

// Pull these properties from EntityBase if not overridden
std::string MQTTComponent::friendly_name() const { return this->get_entity()->get_name(); }
std::string MQTTComponent::get_icon() const { return this->get_entity()->get_icon(); }
bool MQTTComponent::is_disabled_by_default() const { return this->get_entity()->is_disabled_by_default(); }
bool MQTTComponent::is_internal() {
  if (this->has_custom_state_topic_) {
    // If the custom state_topic is null, return true as it is internal and should not publish
    // else, return false, as it is explicitly set to a topic, so it is not internal and should publish
    return this->get_state_topic_().empty();
  }

  if (this->has_custom_command_topic_) {
    // If the custom command_topic is null, return true as it is internal and should not publish
    // else, return false, as it is explicitly set to a topic, so it is not internal and should publish
    return this->get_command_topic_().empty();
  }

  // No custom topics have been set
  if (this->get_default_topic_for_("").empty()) {
    // If the default topic prefix is null, then the component, by default, is internal and should not publish
    return true;
  }

  // Use ESPHome's component internal state if topic_prefix is not null with no custom state_topic or command_topic
  return this->get_entity()->is_internal();
}

}  // namespace mqtt
}  // namespace esphome

#endif  // USE_MQTT
