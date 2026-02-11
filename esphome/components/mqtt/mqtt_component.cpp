#include "mqtt_component.h"

#ifdef USE_MQTT

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include "esphome/core/version.h"

#include "mqtt_const.h"

namespace esphome::mqtt {

static const char *const TAG = "mqtt.component";

// Entity category MQTT strings indexed by EntityCategory enum: NONE(0) is skipped, CONFIG(1), DIAGNOSTIC(2)
PROGMEM_STRING_TABLE(EntityCategoryMqttStrings, "", "config", "diagnostic");

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
// MQTT_COMPONENT_TYPE_MAX_LEN, MQTT_SUFFIX_MAX_LEN, and MQTT_DEFAULT_TOPIC_MAX_LEN are in mqtt_component.h.
// ESPHOME_DEVICE_NAME_MAX_LEN and OBJECT_ID_MAX_LEN are defined in entity_base.h.
// This ensures the stack buffers below are always large enough.
// MQTT_DISCOVERY_PREFIX_MAX_LEN and MQTT_DISCOVERY_TOPIC_MAX_LEN are defined in mqtt_component.h

// Function implementation of LOG_MQTT_COMPONENT macro to reduce code size
void log_mqtt_component(const char *tag, MQTTComponent *obj, bool state_topic, bool command_topic) {
  char buf[MQTT_DEFAULT_TOPIC_MAX_LEN];
  if (state_topic)
    ESP_LOGCONFIG(tag, "  State Topic: '%s'", obj->get_state_topic_to_(buf).c_str());
  if (command_topic)
    ESP_LOGCONFIG(tag, "  Command Topic: '%s'", obj->get_command_topic_to_(buf).c_str());
}

void MQTTComponent::set_qos(uint8_t qos) { this->qos_ = qos; }

void MQTTComponent::set_subscribe_qos(uint8_t qos) { this->subscribe_qos_ = qos; }

void MQTTComponent::set_retain(bool retain) { this->retain_ = retain; }

StringRef MQTTComponent::get_discovery_topic_to_(std::span<char, MQTT_DISCOVERY_TOPIC_MAX_LEN> buf,
                                                 const MQTTDiscoveryInfo &discovery_info) const {
  char sanitized_name[ESPHOME_DEVICE_NAME_MAX_LEN + 1];
  str_sanitize_to(sanitized_name, App.get_name().c_str());
  const char *comp_type = this->component_type();
  char object_id_buf[OBJECT_ID_MAX_LEN];
  StringRef object_id = this->get_default_object_id_to_(object_id_buf);

  char *p = buf.data();

  p = append_str(p, discovery_info.prefix.data(), discovery_info.prefix.size());
  p = append_char(p, '/');
  p = append_str(p, comp_type, strlen(comp_type));
  p = append_char(p, '/');
  p = append_str(p, sanitized_name, strlen(sanitized_name));
  p = append_char(p, '/');
  p = append_str(p, object_id.c_str(), object_id.size());
  p = append_str(p, "/config", 7);
  *p = '\0';

  return StringRef(buf.data(), p - buf.data());
}

StringRef MQTTComponent::get_default_topic_for_to_(std::span<char, MQTT_DEFAULT_TOPIC_MAX_LEN> buf, const char *suffix,
                                                   size_t suffix_len) const {
  const std::string &topic_prefix = global_mqtt_client->get_topic_prefix();
  if (topic_prefix.empty()) {
    return StringRef();  // Empty topic_prefix means no default topic
  }

  const char *comp_type = this->component_type();
  char object_id_buf[OBJECT_ID_MAX_LEN];
  StringRef object_id = this->get_default_object_id_to_(object_id_buf);

  char *p = buf.data();

  p = append_str(p, topic_prefix.data(), topic_prefix.size());
  p = append_char(p, '/');
  p = append_str(p, comp_type, strlen(comp_type));
  p = append_char(p, '/');
  p = append_str(p, object_id.c_str(), object_id.size());
  p = append_char(p, '/');
  p = append_str(p, suffix, suffix_len);
  *p = '\0';

  return StringRef(buf.data(), p - buf.data());
}

std::string MQTTComponent::get_default_topic_for_(const std::string &suffix) const {
  char buf[MQTT_DEFAULT_TOPIC_MAX_LEN];
  StringRef ref = this->get_default_topic_for_to_(buf, suffix.data(), suffix.size());
  return std::string(ref.c_str(), ref.size());
}

StringRef MQTTComponent::get_state_topic_to_(std::span<char, MQTT_DEFAULT_TOPIC_MAX_LEN> buf) const {
  if (this->custom_state_topic_.has_value()) {
    // Returns ref to existing data for static/value, uses buf only for lambda case
    return this->custom_state_topic_.ref_or_copy_to(buf.data(), buf.size());
  }
  return this->get_default_topic_for_to_(buf, "state", 5);
}

StringRef MQTTComponent::get_command_topic_to_(std::span<char, MQTT_DEFAULT_TOPIC_MAX_LEN> buf) const {
  if (this->custom_command_topic_.has_value()) {
    // Returns ref to existing data for static/value, uses buf only for lambda case
    return this->custom_command_topic_.ref_or_copy_to(buf.data(), buf.size());
  }
  return this->get_default_topic_for_to_(buf, "command", 7);
}

std::string MQTTComponent::get_state_topic_() const {
  char buf[MQTT_DEFAULT_TOPIC_MAX_LEN];
  StringRef ref = this->get_state_topic_to_(buf);
  return std::string(ref.c_str(), ref.size());
}

std::string MQTTComponent::get_command_topic_() const {
  char buf[MQTT_DEFAULT_TOPIC_MAX_LEN];
  StringRef ref = this->get_command_topic_to_(buf);
  return std::string(ref.c_str(), ref.size());
}

bool MQTTComponent::publish(const std::string &topic, const std::string &payload) {
  return this->publish(topic.c_str(), payload.data(), payload.size());
}

bool MQTTComponent::publish(const std::string &topic, const char *payload, size_t payload_length) {
  return this->publish(topic.c_str(), payload, payload_length);
}

bool MQTTComponent::publish(const char *topic, const char *payload, size_t payload_length) {
  if (topic[0] == '\0')
    return false;
  return global_mqtt_client->publish(topic, payload, payload_length, this->qos_, this->retain_);
}

bool MQTTComponent::publish(const char *topic, const char *payload) {
  return this->publish(topic, payload, strlen(payload));
}

#ifdef USE_ESP8266
bool MQTTComponent::publish(const std::string &topic, ProgmemStr payload) {
  return this->publish(topic.c_str(), payload);
}

bool MQTTComponent::publish(const char *topic, ProgmemStr payload) {
  if (topic[0] == '\0')
    return false;
  // On ESP8266, ProgmemStr is __FlashStringHelper* - need to copy from flash
  char buf[64];
  strncpy_P(buf, reinterpret_cast<const char *>(payload), sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  return global_mqtt_client->publish(topic, buf, strlen(buf), this->qos_, this->retain_);
}
#endif

bool MQTTComponent::publish_json(const std::string &topic, const json::json_build_t &f) {
  return this->publish_json(topic.c_str(), f);
}

bool MQTTComponent::publish_json(const char *topic, const json::json_build_t &f) {
  if (topic[0] == '\0')
    return false;
  return global_mqtt_client->publish_json(topic, f, this->qos_, this->retain_);
}

bool MQTTComponent::send_discovery_() {
  const MQTTDiscoveryInfo &discovery_info = global_mqtt_client->get_discovery_info();

  char discovery_topic_buf[MQTT_DISCOVERY_TOPIC_MAX_LEN];
  StringRef discovery_topic = this->get_discovery_topic_to_(discovery_topic_buf, discovery_info);

  if (discovery_info.clean) {
    ESP_LOGV(TAG, "'%s': Cleaning discovery", this->friendly_name_().c_str());
    return global_mqtt_client->publish(discovery_topic.c_str(), "", 0, this->qos_, true);
  }

  ESP_LOGV(TAG, "'%s': Sending discovery", this->friendly_name_().c_str());

  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  return global_mqtt_client->publish_json(
      discovery_topic.c_str(),
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
        root[MQTT_NAME] = this->get_entity()->has_own_name() ? this->friendly_name_() : StringRef();

        if (this->is_disabled_by_default_())
          root[MQTT_ENABLED_BY_DEFAULT] = false;
        // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
        const auto icon_ref = this->get_icon_ref_();
        if (!icon_ref.empty()) {
          root[MQTT_ICON] = icon_ref;
        }
        // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

        const auto entity_category = this->get_entity()->get_entity_category();
        if (entity_category != ENTITY_CATEGORY_NONE) {
          root[MQTT_ENTITY_CATEGORY] = EntityCategoryMqttStrings::get_progmem_str(
              static_cast<uint8_t>(entity_category), static_cast<uint8_t>(ENTITY_CATEGORY_CONFIG));
        }

        if (config.state_topic) {
          char state_topic_buf[MQTT_DEFAULT_TOPIC_MAX_LEN];
          root[MQTT_STATE_TOPIC] = this->get_state_topic_to_(state_topic_buf);
        }
        if (config.command_topic) {
          char command_topic_buf[MQTT_DEFAULT_TOPIC_MAX_LEN];
          root[MQTT_COMMAND_TOPIC] = this->get_command_topic_to_(command_topic_buf);
        }
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
          buf_append_printf(friendly_name_hash, sizeof(friendly_name_hash), 0, "%08" PRIx32,
                            fnv1_hash(this->friendly_name_().c_str()));
          // Format: mac-component_type-hash (e.g. "aabbccddeeff-sensor-12345678")
          // MAC (12) + "-" (1) + domain (max 20) + "-" (1) + hash (8) + null (1) = 43
          char unique_id[MAC_ADDRESS_BUFFER_SIZE + ESPHOME_DOMAIN_MAX_LEN + 11];
          char mac_buf[MAC_ADDRESS_BUFFER_SIZE];
          get_mac_address_into_buffer(mac_buf);
          buf_append_printf(unique_id, sizeof(unique_id), 0, "%s-%s-%s", mac_buf, this->component_type(),
                            friendly_name_hash);
          root[MQTT_UNIQUE_ID] = unique_id;
        } else {
          // default to almost-unique ID. It's a hack but the only way to get that
          // gorgeous device registry view.
          // "ESP" (3) + component_type (max 20) + object_id (max 128) + null
          char unique_id_buf[3 + MQTT_COMPONENT_TYPE_MAX_LEN + OBJECT_ID_MAX_LEN + 1];
          buf_append_printf(unique_id_buf, sizeof(unique_id_buf), 0, "ESP%s%s", this->component_type(),
                            object_id.c_str());
          root[MQTT_UNIQUE_ID] = unique_id_buf;
        }

        const std::string &node_name = App.get_name();
        if (discovery_info.object_id_generator == MQTT_DEVICE_NAME_OBJECT_ID_GENERATOR) {
          // node_name (max 31) + "_" (1) + object_id (max 128) + null
          char object_id_full[ESPHOME_DEVICE_NAME_MAX_LEN + 1 + OBJECT_ID_MAX_LEN + 1];
          buf_append_printf(object_id_full, sizeof(object_id_full), 0, "%s_%s", node_name.c_str(), object_id.c_str());
          root[MQTT_OBJECT_ID] = object_id_full;
        }

        const std::string &friendly_name_ref = App.get_friendly_name();
        const std::string &node_friendly_name = friendly_name_ref.empty() ? node_name : friendly_name_ref;
        const char *node_area = App.get_area();

        JsonObject device_info = root[MQTT_DEVICE].to<JsonObject>();
        char mac[MAC_ADDRESS_BUFFER_SIZE];
        get_mac_address_into_buffer(mac);
        device_info[MQTT_DEVICE_IDENTIFIERS] = mac;
        device_info[MQTT_DEVICE_NAME] = node_friendly_name;
#ifdef ESPHOME_PROJECT_NAME
        device_info[MQTT_DEVICE_SW_VERSION] = ESPHOME_PROJECT_VERSION " (ESPHome " ESPHOME_VERSION ")";
        const char *model = std::strchr(ESPHOME_PROJECT_NAME, '.');
        device_info[MQTT_DEVICE_MODEL] = model == nullptr ? ESPHOME_BOARD : model + 1;
        if (model == nullptr) {
          device_info[MQTT_DEVICE_MANUFACTURER] = ESPHOME_PROJECT_NAME;
        } else {
          // Extract manufacturer (part before '.') using stack buffer to avoid heap allocation
          // memcpy is used instead of strncpy since we know the exact length and strncpy
          // would still require manual null-termination
          char manufacturer[sizeof(ESPHOME_PROJECT_NAME)];
          size_t len = model - ESPHOME_PROJECT_NAME;
          memcpy(manufacturer, ESPHOME_PROJECT_NAME, len);
          manufacturer[len] = '\0';
          device_info[MQTT_DEVICE_MANUFACTURER] = manufacturer;
        }
#else
        static const char ver_fmt[] PROGMEM = ESPHOME_VERSION " (config hash 0x%08" PRIx32 ")";
        // Buffer sized for format string expansion: ~4 bytes net growth from format specifier to 8 hex digits, plus
        // safety margin
        char version_buf[sizeof(ver_fmt) + 8];
#ifdef USE_ESP8266
        snprintf_P(version_buf, sizeof(version_buf), ver_fmt, App.get_config_hash());
#else
        snprintf(version_buf, sizeof(version_buf), ver_fmt, App.get_config_hash());
#endif
        device_info[MQTT_DEVICE_SW_VERSION] = version_buf;
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
        if (node_area[0] != '\0') {
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
  // Cache is_internal result once during setup - topics don't change after this
  this->is_internal_ = this->compute_is_internal_();
  if (this->is_internal_)
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

void MQTTComponent::process_resend() {
  // Called by MQTTClientComponent when connected to process pending resends
  // Note: is_internal() check not needed - internal components are never registered
  if (!this->resend_state_)
    return;

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
const StringRef &MQTTComponent::friendly_name_() const { return this->get_entity()->get_name(); }
StringRef MQTTComponent::get_default_object_id_to_(std::span<char, OBJECT_ID_MAX_LEN> buf) const {
  return this->get_entity()->get_object_id_to(buf);
}
StringRef MQTTComponent::get_icon_ref_() const { return this->get_entity()->get_icon_ref(); }
bool MQTTComponent::is_disabled_by_default_() const { return this->get_entity()->is_disabled_by_default(); }
bool MQTTComponent::compute_is_internal_() {
  if (this->custom_state_topic_.has_value()) {
    // If the custom state_topic is empty, return true as it is internal and should not publish
    // else, return false, as it is explicitly set to a topic, so it is not internal and should publish
    // Using is_empty() avoids heap allocation for non-lambda cases
    return this->custom_state_topic_.is_empty();
  }

  if (this->custom_command_topic_.has_value()) {
    // If the custom command_topic is empty, return true as it is internal and should not publish
    // else, return false, as it is explicitly set to a topic, so it is not internal and should publish
    // Using is_empty() avoids heap allocation for non-lambda cases
    return this->custom_command_topic_.is_empty();
  }

  // No custom topics have been set - check topic_prefix directly to avoid allocation
  if (global_mqtt_client->get_topic_prefix().empty()) {
    // If the default topic prefix is empty, then the component, by default, is internal and should not publish
    return true;
  }

  // Use ESPHome's component internal state if topic_prefix is not empty with no custom state_topic or command_topic
  return this->get_entity()->is_internal();
}

}  // namespace esphome::mqtt

#endif  // USE_MQTT
