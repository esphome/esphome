#include "mqtt_component.h"

#ifdef USE_MQTT

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"

#include "mqtt_const.h"

namespace esphome::mqtt {

static const char *const TAG = "mqtt.component";

// Helper functions for building topic strings on stack
inline char *append_str(char *p, const char *s, size_t len) {
  memcpy(p, s, len);
  return p + len;
}

inline char *append_char(char *p, char c) {
  *p = c;
  return p + 1;
}

// Max lengths for stack-based topic building.
// These limits are enforced at Python config validation time in mqtt/__init__.py
// using cv.Length() validators for topic_prefix and discovery_prefix.
// MQTT_COMPONENT_TYPE_MAX_LEN and MQTT_SUFFIX_MAX_LEN are defined in mqtt_component.h.
// ESPHOME_DEVICE_NAME_MAX_LEN and OBJECT_ID_MAX_LEN are defined in entity_base.h.
// This ensures the stack buffers below are always large enough.
static constexpr size_t TOPIC_PREFIX_MAX_LEN = 64;      // Validated in Python: cv.Length(max=64)
static constexpr size_t DISCOVERY_PREFIX_MAX_LEN = 64;  // Validated in Python: cv.Length(max=64)

// Stack buffer sizes - safe because all inputs are length-validated at config time
// Format: prefix + "/" + type + "/" + object_id + "/" + suffix + null
static constexpr size_t DEFAULT_TOPIC_MAX_LEN =
    TOPIC_PREFIX_MAX_LEN + 1 + MQTT_COMPONENT_TYPE_MAX_LEN + 1 + OBJECT_ID_MAX_LEN + 1 + MQTT_SUFFIX_MAX_LEN + 1;
// Format: prefix + "/" + type + "/" + name + "/" + object_id + "/config" + null
static constexpr size_t DISCOVERY_TOPIC_MAX_LEN = DISCOVERY_PREFIX_MAX_LEN + 1 + MQTT_COMPONENT_TYPE_MAX_LEN + 1 +
                                                  ESPHOME_DEVICE_NAME_MAX_LEN + 1 + OBJECT_ID_MAX_LEN + 7 + 1;

void MQTTComponent::set_qos(uint8_t qos) { this->qos_ = qos; }

void MQTTComponent::set_subscribe_qos(uint8_t qos) { this->subscribe_qos_ = qos; }

void MQTTComponent::set_retain(bool retain) { this->retain_ = retain; }

std::string MQTTComponent::get_discovery_topic_(const MQTTDiscoveryInfo &discovery_info) const {
  std::string sanitized_name = str_sanitize(App.get_name());
  const char *comp_type = this->component_type();
  char object_id_buf[OBJECT_ID_MAX_LEN];
  StringRef object_id = this->get_default_object_id_to_(object_id_buf);

  char buf[DISCOVERY_TOPIC_MAX_LEN];
  char *p = buf;

  p = append_str(p, discovery_info.prefix.data(), discovery_info.prefix.size());
  p = append_char(p, '/');
  p = append_str(p, comp_type, strlen(comp_type));
  p = append_char(p, '/');
  p = append_str(p, sanitized_name.data(), sanitized_name.size());
  p = append_char(p, '/');
  p = append_str(p, object_id.c_str(), object_id.size());
  p = append_str(p, "/config", 7);

  return std::string(buf, p - buf);
}

std::string MQTTComponent::get_default_topic_for_(const std::string &suffix) const {
  const std::string &topic_prefix = global_mqtt_client->get_topic_prefix();
  if (topic_prefix.empty()) {
    // If the topic_prefix is null, the default topic should be null
    return "";
  }

  const char *comp_type = this->component_type();
  char object_id_buf[OBJECT_ID_MAX_LEN];
  StringRef object_id = this->get_default_object_id_to_(object_id_buf);

  char buf[DEFAULT_TOPIC_MAX_LEN];
  char *p = buf;

  p = append_str(p, topic_prefix.data(), topic_prefix.size());
  p = append_char(p, '/');
  p = append_str(p, comp_type, strlen(comp_type));
  p = append_char(p, '/');
  p = append_str(p, object_id.c_str(), object_id.size());
  p = append_char(p, '/');
  p = append_str(p, suffix.data(), suffix.size());

  return std::string(buf, p - buf);
}

std::string MQTTComponent::get_state_topic_() const {
  if (this->custom_state_topic_.has_value())
    return this->custom_state_topic_.value();
  return this->get_default_topic_for_("state");
}

std::string MQTTComponent::get_command_topic_() const {
  if (this->custom_command_topic_.has_value())
    return this->custom_command_topic_.value();
  return this->get_default_topic_for_("command");
}

bool MQTTComponent::publish(const std::string &topic, const std::string &payload) {
  return this->publish(topic, payload.data(), payload.size());
}

bool MQTTComponent::publish(const std::string &topic, const char *payload, size_t payload_length) {
  if (topic.empty())
    return false;
  return global_mqtt_client->publish(topic, payload, payload_length, this->qos_, this->retain_);
}

bool MQTTComponent::publish_json(const std::string &topic, const json::json_build_t &f) {
  if (topic.empty())
    return false;
  return global_mqtt_client->publish_json(topic, f, this->qos_, this->retain_);
}

bool MQTTComponent::send_discovery_() {
  const MQTTDiscoveryInfo &discovery_info = global_mqtt_client->get_discovery_info();

  if (discovery_info.clean) {
    ESP_LOGV(TAG, "'%s': Cleaning discovery", this->friendly_name_().c_str());
    return global_mqtt_client->publish(this->get_discovery_topic_(discovery_info), "", 0, this->qos_, true);
  }

  ESP_LOGV(TAG, "'%s': Sending discovery", this->friendly_name_().c_str());

  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  return global_mqtt_client->publish_json(
      this->get_discovery_topic_(discovery_info),
      [this](JsonObject root) {
        SendDiscoveryConfig config;
        config.state_topic = true;
        config.command_topic = true;

        this->send_discovery(root, config);
        // Set subscription QoS (default is 0)
        if (this->subscribe_qos_ != 0) {
          root[MQTT_QOS] = this->subscribe_qos_;
        }

        // Fields from EntityBase
        root[MQTT_NAME] = this->get_entity()->has_own_name() ? this->friendly_name_() : "";

        if (this->is_disabled_by_default_())
          root[MQTT_ENABLED_BY_DEFAULT] = false;
        // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
        const auto icon_ref = this->get_icon_ref_();
        if (!icon_ref.empty()) {
          root[MQTT_ICON] = icon_ref;
        }
        // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

        const auto entity_category = this->get_entity()->get_entity_category();
        switch (entity_category) {
          case ENTITY_CATEGORY_NONE:
            break;
          case ENTITY_CATEGORY_CONFIG:
          case ENTITY_CATEGORY_DIAGNOSTIC:
            root[MQTT_ENTITY_CATEGORY] = entity_category == ENTITY_CATEGORY_CONFIG ? "config" : "diagnostic";
            break;
        }

        if (config.state_topic)
          root[MQTT_STATE_TOPIC] = this->get_state_topic_();
        if (config.command_topic)
          root[MQTT_COMMAND_TOPIC] = this->get_command_topic_();
        if (this->command_retain_)
          root[MQTT_COMMAND_RETAIN] = true;

        const Availability &avail =
            this->availability_ == nullptr ? global_mqtt_client->get_availability() : *this->availability_;
        if (!avail.topic.empty()) {
          root[MQTT_AVAILABILITY_TOPIC] = avail.topic;
          if (avail.payload_available != "online")
            root[MQTT_PAYLOAD_AVAILABLE] = avail.payload_available;
          if (avail.payload_not_available != "offline")
            root[MQTT_PAYLOAD_NOT_AVAILABLE] = avail.payload_not_available;
        }

        const MQTTDiscoveryInfo &discovery_info = global_mqtt_client->get_discovery_info();
        char object_id_buf[OBJECT_ID_MAX_LEN];
        StringRef object_id = this->get_default_object_id_to_(object_id_buf);
        if (discovery_info.unique_id_generator == MQTT_MAC_ADDRESS_UNIQUE_ID_GENERATOR) {
          char friendly_name_hash[9];
          sprintf(friendly_name_hash, "%08" PRIx32, fnv1_hash(this->friendly_name_()));
          friendly_name_hash[8] = 0;  // ensure the hash-string ends with null
          // Format: mac-component_type-hash (e.g. "aabbccddeeff-sensor-12345678")
          // MAC (12) + "-" (1) + domain (max 20) + "-" (1) + hash (8) + null (1) = 43
          char unique_id[MAC_ADDRESS_BUFFER_SIZE + ESPHOME_DOMAIN_MAX_LEN + 11];
          char mac_buf[MAC_ADDRESS_BUFFER_SIZE];
          get_mac_address_into_buffer(mac_buf);
          snprintf(unique_id, sizeof(unique_id), "%s-%s-%s", mac_buf, this->component_type(), friendly_name_hash);
          root[MQTT_UNIQUE_ID] = unique_id;
        } else {
          // default to almost-unique ID. It's a hack but the only way to get that
          // gorgeous device registry view.
          root[MQTT_UNIQUE_ID] = "ESP" + std::string(this->component_type()) + object_id.c_str();
        }

        const std::string &node_name = App.get_name();
        if (discovery_info.object_id_generator == MQTT_DEVICE_NAME_OBJECT_ID_GENERATOR)
          root[MQTT_OBJECT_ID] = node_name + "_" + object_id.c_str();

        const std::string &friendly_name_ref = App.get_friendly_name();
        const std::string &node_friendly_name = friendly_name_ref.empty() ? node_name : friendly_name_ref;
        std::string node_area = App.get_area();

        JsonObject device_info = root[MQTT_DEVICE].to<JsonObject>();
        char mac[MAC_ADDRESS_BUFFER_SIZE];
        get_mac_address_into_buffer(mac);
        device_info[MQTT_DEVICE_IDENTIFIERS] = mac;
        device_info[MQTT_DEVICE_NAME] = node_friendly_name;
#ifdef ESPHOME_PROJECT_NAME
        device_info[MQTT_DEVICE_SW_VERSION] = ESPHOME_PROJECT_VERSION " (ESPHome " ESPHOME_VERSION ")";
        const char *model = std::strchr(ESPHOME_PROJECT_NAME, '.');
        device_info[MQTT_DEVICE_MODEL] = model == nullptr ? ESPHOME_BOARD : model + 1;
        device_info[MQTT_DEVICE_MANUFACTURER] =
            model == nullptr ? ESPHOME_PROJECT_NAME : std::string(ESPHOME_PROJECT_NAME, model - ESPHOME_PROJECT_NAME);
#else
        static const char ver_fmt[] PROGMEM = ESPHOME_VERSION " (config hash 0x%08" PRIx32 ")";
#ifdef USE_ESP8266
        char fmt_buf[sizeof(ver_fmt)];
        strcpy_P(fmt_buf, ver_fmt);
        const char *fmt = fmt_buf;
#else
        const char *fmt = ver_fmt;
#endif
        device_info[MQTT_DEVICE_SW_VERSION] = str_sprintf(fmt, App.get_config_hash());
        device_info[MQTT_DEVICE_MODEL] = ESPHOME_BOARD;
#if defined(USE_ESP8266) || defined(USE_ESP32)
        device_info[MQTT_DEVICE_MANUFACTURER] = "Espressif";
#elif defined(USE_RP2040)
        device_info[MQTT_DEVICE_MANUFACTURER] = "Raspberry Pi";
#elif defined(USE_BK72XX)
        device_info[MQTT_DEVICE_MANUFACTURER] = "Beken";
#elif defined(USE_RTL87XX)
        device_info[MQTT_DEVICE_MANUFACTURER] = "Realtek";
#elif defined(USE_HOST)
        device_info[MQTT_DEVICE_MANUFACTURER] = "Host";
#endif
#endif
        if (!node_area.empty()) {
          device_info[MQTT_DEVICE_SUGGESTED_AREA] = node_area;
        }

        device_info[MQTT_DEVICE_CONNECTIONS][0][0] = "mac";
        device_info[MQTT_DEVICE_CONNECTIONS][0][1] = mac;
      },
      this->qos_, discovery_info.retain);
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

uint8_t MQTTComponent::get_qos() const { return this->qos_; }

bool MQTTComponent::get_retain() const { return this->retain_; }

bool MQTTComponent::is_discovery_enabled() const {
  return this->discovery_enabled_ && global_mqtt_client->is_discovery_enabled();
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
bool MQTTComponent::is_connected_() const { return global_mqtt_client->is_connected(); }

// Pull these properties from EntityBase if not overridden
std::string MQTTComponent::friendly_name_() const { return this->get_entity()->get_name(); }
StringRef MQTTComponent::get_default_object_id_to_(std::span<char, OBJECT_ID_MAX_LEN> buf) const {
  return this->get_entity()->get_object_id_to(buf);
}
StringRef MQTTComponent::get_icon_ref_() const { return this->get_entity()->get_icon_ref(); }
bool MQTTComponent::is_disabled_by_default_() const { return this->get_entity()->is_disabled_by_default(); }
bool MQTTComponent::is_internal() {
  if (this->custom_state_topic_.has_value()) {
    // If the custom state_topic is null, return true as it is internal and should not publish
    // else, return false, as it is explicitly set to a topic, so it is not internal and should publish
    return this->get_state_topic_().empty();
  }

  if (this->custom_command_topic_.has_value()) {
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

}  // namespace esphome::mqtt

#endif  // USE_MQTT
