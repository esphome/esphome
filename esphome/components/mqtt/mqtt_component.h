#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT

#include <memory>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/progmem.h"
#include "esphome/core/string_ref.h"
#include "mqtt_client.h"

namespace esphome::mqtt {

/// Simple Helper struct used for Home Assistant MQTT send_discovery().
struct SendDiscoveryConfig {
  bool state_topic{true};    ///< If the state topic should be included. Defaults to true.
  bool command_topic{true};  ///< If the command topic should be included. Default to true.
};

// Max lengths for stack-based topic building.
// These limits are enforced at Python config validation time in mqtt/__init__.py
// using cv.Length() validators for topic_prefix and discovery_prefix.
// This ensures the stack buffers are always large enough.
static constexpr size_t MQTT_COMPONENT_TYPE_MAX_LEN = 20;
static constexpr size_t MQTT_SUFFIX_MAX_LEN = 32;
static constexpr size_t MQTT_TOPIC_PREFIX_MAX_LEN = 64;  // Validated in Python: cv.Length(max=64)
// Stack buffer size - safe because all inputs are length-validated at config time
// Format: prefix + "/" + type + "/" + object_id + "/" + suffix + null
static constexpr size_t MQTT_DEFAULT_TOPIC_MAX_LEN =
    MQTT_TOPIC_PREFIX_MAX_LEN + 1 + MQTT_COMPONENT_TYPE_MAX_LEN + 1 + OBJECT_ID_MAX_LEN + 1 + MQTT_SUFFIX_MAX_LEN + 1;
static constexpr size_t MQTT_DISCOVERY_PREFIX_MAX_LEN = 64;  // Validated in Python: cv.Length(max=64)
// Format: prefix + "/" + type + "/" + name + "/" + object_id + "/config" + null
static constexpr size_t MQTT_DISCOVERY_TOPIC_MAX_LEN = MQTT_DISCOVERY_PREFIX_MAX_LEN + 1 + MQTT_COMPONENT_TYPE_MAX_LEN +
                                                       1 + ESPHOME_DEVICE_NAME_MAX_LEN + 1 + OBJECT_ID_MAX_LEN + 7 + 1;

class MQTTComponent;  // Forward declaration
void log_mqtt_component(const char *tag, MQTTComponent *obj, bool state_topic, bool command_topic);

#define LOG_MQTT_COMPONENT(state_topic, command_topic) log_mqtt_component(TAG, this, state_topic, command_topic)

// Macro to define component_type() with compile-time length verification
// Usage: MQTT_COMPONENT_TYPE(MQTTSensorComponent, "sensor")
#define MQTT_COMPONENT_TYPE(class_name, type_str) \
  const char *class_name::component_type() const { return type_str; } \
  static_assert(sizeof(type_str) - 1 <= MQTT_COMPONENT_TYPE_MAX_LEN, \
                #class_name "::component_type() exceeds MQTT_COMPONENT_TYPE_MAX_LEN");

// Macro to define custom topic getter/setter with compile-time suffix length verification
#define MQTT_COMPONENT_CUSTOM_TOPIC_(name, type) \
  static_assert(sizeof(#name "/" #type) - 1 <= MQTT_SUFFIX_MAX_LEN, \
                "topic suffix " #name "/" #type " exceeds MQTT_SUFFIX_MAX_LEN"); \
\
 protected: \
  std::string custom_##name##_##type##_topic_{}; \
\
 public: \
  void set_custom_##name##_##type##_topic(const std::string &topic) { this->custom_##name##_##type##_topic_ = topic; } \
  std::string get_##name##_##type##_topic() const { \
    if (this->custom_##name##_##type##_topic_.empty()) \
      return this->get_default_topic_for_(#name "/" #type); \
    return this->custom_##name##_##type##_topic_; \
  }

#define MQTT_COMPONENT_CUSTOM_TOPIC(name, type) MQTT_COMPONENT_CUSTOM_TOPIC_(name, type)

/** MQTTComponent is the base class for all components that interact with MQTT to expose
 * certain functionality or data from actuators or sensors to clients.
 *
 * Although this class should work with all MQTT solutions, it has been specifically designed for use
 * with Home Assistant. For example, this class supports Home Assistant MQTT discovery out of the box.
 *
 * In order to implement automatic Home Assistant discovery, all sub-classes should:
 *
 *  1. Implement send_discovery that creates a Home Assistant discovery payload.
 *  2. Override component_type() to return the appropriate component type such as "light" or "sensor".
 *  3. Subscribe to command topics using subscribe() or subscribe_json() during setup().
 *
 * In order to best separate the front- and back-end of ESPHome, all sub-classes should
 * only parse/send MQTT messages and interact with back-end components via callbacks to ensure
 * a clean separation.
 */
class MQTTComponent : public Component {
  friend void log_mqtt_component(const char *tag, MQTTComponent *obj, bool state_topic, bool command_topic);

 public:
  /// Constructs a MQTTComponent.
  explicit MQTTComponent();

  /// Override setup_ so that we can call send_discovery() when needed.
  void call_setup() override;

  void call_dump_config() override;

  /// Send discovery info the Home Assistant, override this.
  virtual void send_discovery(JsonObject root, SendDiscoveryConfig &config) = 0;

  virtual bool send_initial_state() = 0;

  /// Returns cached is_internal result (computed once during setup).
  bool is_internal() const { return this->is_internal_; }

  /// Set QOS for state messages.
  void set_qos(uint8_t qos);
  uint8_t get_qos() const;

  /// Set whether state message should be retained.
  void set_retain(bool retain);
  bool get_retain() const;

  /// Disable discovery. Sets friendly name to "".
  void disable_discovery();
  bool is_discovery_enabled() const;

  /// Set the QOS for subscribe messages (used in discovery).
  void set_subscribe_qos(uint8_t qos);

  /// Override this method to return the component type (e.g. "light", "sensor", ...)
  virtual const char *component_type() const = 0;

  /// Set a custom state topic. Do not set for default behavior.
  template<typename T> void set_custom_state_topic(T &&custom_state_topic) {
    this->custom_state_topic_ = std::forward<T>(custom_state_topic);
  }
  template<typename T> void set_custom_command_topic(T &&custom_command_topic) {
    this->custom_command_topic_ = std::forward<T>(custom_command_topic);
  }
  /// Set whether command message should be retained.
  void set_command_retain(bool command_retain);

  /// MQTT_COMPONENT setup priority.
  float get_setup_priority() const override;

  /** Set the Home Assistant availability data.
   *
   * See See <a href="https://www.home-assistant.io/components/binary_sensor.mqtt/">Home Assistant</a> for more info.
   */
  void set_availability(std::string topic, std::string payload_available, std::string payload_not_available);
  void disable_availability();

  /// Internal method for the MQTT client base to schedule a resend of the state on reconnect.
  void schedule_resend_state();

  /// Process pending resend if needed (called by MQTTClientComponent)
  void process_resend();

  /** Send a MQTT message.
   *
   * @param topic The topic.
   * @param payload The payload.
   */
  bool publish(const std::string &topic, const std::string &payload);

  /** Send a MQTT message.
   *
   * @param topic The topic.
   * @param payload The payload buffer.
   * @param payload_length The length of the payload.
   */
  bool publish(const std::string &topic, const char *payload, size_t payload_length);

  /** Send a MQTT message.
   *
   * @param topic The topic.
   * @param payload The null-terminated payload.
   */
  bool publish(const std::string &topic, const char *payload) {
    return this->publish(topic.c_str(), payload, strlen(payload));
  }

  /** Send a MQTT message (no heap allocation for topic).
   *
   * @param topic The topic as C string.
   * @param payload The payload buffer.
   * @param payload_length The length of the payload.
   */
  bool publish(const char *topic, const char *payload, size_t payload_length);

  /** Send a MQTT message (no heap allocation for topic).
   *
   * @param topic The topic as StringRef (for use with get_state_topic_to_()).
   * @param payload The payload buffer.
   * @param payload_length The length of the payload.
   */
  bool publish(StringRef topic, const char *payload, size_t payload_length) {
    return this->publish(topic.c_str(), payload, payload_length);
  }

  /** Send a MQTT message (no heap allocation for topic).
   *
   * @param topic The topic as C string.
   * @param payload The null-terminated payload.
   */
  bool publish(const char *topic, const char *payload);

  /** Send a MQTT message (no heap allocation for topic).
   *
   * @param topic The topic as StringRef (for use with get_state_topic_to_()).
   * @param payload The null-terminated payload.
   */
  bool publish(StringRef topic, const char *payload) { return this->publish(topic.c_str(), payload); }

#ifdef USE_ESP8266
  /** Send a MQTT message with a PROGMEM string payload.
   *
   * @param topic The topic.
   * @param payload The payload (ProgmemStr - stored in flash on ESP8266).
   */
  bool publish(const std::string &topic, ProgmemStr payload);

  /** Send a MQTT message with a PROGMEM string payload (no heap allocation for topic).
   *
   * @param topic The topic as C string.
   * @param payload The payload (ProgmemStr - stored in flash on ESP8266).
   */
  bool publish(const char *topic, ProgmemStr payload);

  /** Send a MQTT message with a PROGMEM string payload (no heap allocation for topic).
   *
   * @param topic The topic as StringRef (for use with get_state_topic_to_()).
   * @param payload The payload (ProgmemStr - stored in flash on ESP8266).
   */
  bool publish(StringRef topic, ProgmemStr payload) { return this->publish(topic.c_str(), payload); }
#endif

  /** Construct and send a JSON MQTT message.
   *
   * @param topic The topic.
   * @param f The Json Message builder.
   */
  bool publish_json(const std::string &topic, const json::json_build_t &f);

  /** Construct and send a JSON MQTT message (no heap allocation for topic).
   *
   * @param topic The topic as C string.
   * @param f The Json Message builder.
   */
  bool publish_json(const char *topic, const json::json_build_t &f);

  /** Construct and send a JSON MQTT message (no heap allocation for topic).
   *
   * @param topic The topic as StringRef (for use with get_state_topic_to_()).
   * @param f The Json Message builder.
   */
  bool publish_json(StringRef topic, const json::json_build_t &f) { return this->publish_json(topic.c_str(), f); }

  /** Subscribe to a MQTT topic.
   *
   * @param topic The topic. Wildcards are currently not supported.
   * @param callback The callback that will be called when a message with matching topic is received.
   * @param qos The MQTT quality of service. Defaults to 0.
   */
  void subscribe(const std::string &topic, mqtt_callback_t callback, uint8_t qos = 0);

  /** Subscribe to a MQTT topic and automatically parse JSON payload.
   *
   * If an invalid JSON payload is received, the callback will not be called.
   *
   * @param topic The topic. Wildcards are currently not supported.
   * @param callback The callback with a parsed JsonObject that will be called when a message with matching topic is
   * received.
   * @param qos The MQTT quality of service. Defaults to 0.
   */
  void subscribe_json(const std::string &topic, const mqtt_json_callback_t &callback, uint8_t qos = 0);

 protected:
  /// Helper method to get the discovery topic for this component into a buffer.
  StringRef get_discovery_topic_to_(std::span<char, MQTT_DISCOVERY_TOPIC_MAX_LEN> buf,
                                    const MQTTDiscoveryInfo &discovery_info) const;

  /** Get this components state/command/... topic into a buffer.
   *
   * @param buf The buffer to write to (must be exactly MQTT_DEFAULT_TOPIC_MAX_LEN).
   * @param suffix The suffix/key such as "state" or "command".
   * @return StringRef pointing to the buffer with the topic.
   */
  StringRef get_default_topic_for_to_(std::span<char, MQTT_DEFAULT_TOPIC_MAX_LEN> buf, const char *suffix,
                                      size_t suffix_len) const;

  /** Get this components state/command/... topic (allocates std::string).
   *
   * @param suffix The suffix/key such as "state" or "command".
   * @return The full topic.
   */
  std::string get_default_topic_for_(const std::string &suffix) const;

  /**
   * Gets the Entity served by this MQTT component.
   */
  virtual const EntityBase *get_entity() const = 0;

  /// Get the friendly name of this MQTT component.
  const StringRef &friendly_name_() const;

  /// Get the icon field of this component as StringRef
  StringRef get_icon_ref_() const;

  /// Get whether the underlying Entity is disabled by default
  bool is_disabled_by_default_() const;

  /// Get the MQTT state topic into a buffer (no heap allocation for non-lambda custom topics).
  /// @param buf Buffer of exactly MQTT_DEFAULT_TOPIC_MAX_LEN bytes.
  /// @return StringRef pointing to the topic in the buffer.
  StringRef get_state_topic_to_(std::span<char, MQTT_DEFAULT_TOPIC_MAX_LEN> buf) const;

  /// Get the MQTT command topic into a buffer (no heap allocation for non-lambda custom topics).
  /// @param buf Buffer of exactly MQTT_DEFAULT_TOPIC_MAX_LEN bytes.
  /// @return StringRef pointing to the topic in the buffer.
  StringRef get_command_topic_to_(std::span<char, MQTT_DEFAULT_TOPIC_MAX_LEN> buf) const;

  /// Get the MQTT topic that new states will be shared to (allocates std::string).
  std::string get_state_topic_() const;

  /// Get the MQTT topic for listening to commands (allocates std::string).
  std::string get_command_topic_() const;

  bool is_connected_() const;

  /// Internal method to start sending discovery info, this will call send_discovery().
  bool send_discovery_();

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Get the object ID for this MQTT component, writing to the provided buffer.
  StringRef get_default_object_id_to_(std::span<char, OBJECT_ID_MAX_LEN> buf) const;

  TemplatableValue<std::string> custom_state_topic_{};
  TemplatableValue<std::string> custom_command_topic_{};

  std::unique_ptr<Availability> availability_;

  // Packed bitfields - QoS values are 0-2, bools are flags
  uint8_t qos_ : 2 {0};
  uint8_t subscribe_qos_ : 2 {0};
  bool command_retain_ : 1 {false};
  bool retain_ : 1 {true};
  bool discovery_enabled_ : 1 {true};
  bool resend_state_ : 1 {false};
  bool is_internal_ : 1 {false};  ///< Cached result of compute_is_internal_(), set during setup

  /// Compute is_internal status based on topics and entity state.
  /// Called once during setup to cache the result.
  bool compute_is_internal_();
};

}  // namespace esphome::mqtt

#endif  // USE_MQTt
