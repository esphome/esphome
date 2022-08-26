#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT

#include <memory>

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "mqtt_client.h"

namespace esphome {
namespace mqtt {

/// Simple Helper struct used for Home Assistant MQTT send_discovery().
struct SendDiscoveryConfig {
  bool state_topic{true};    ///< If the state topic should be included. Defaults to true.
  bool command_topic{true};  ///< If the command topic should be included. Default to true.
};

#define LOG_MQTT_COMPONENT(state_topic, command_topic) \
  if (state_topic) { \
    ESP_LOGCONFIG(TAG, "  State Topic: '%s'", this->get_state_topic_().c_str()); \
  } \
  if (command_topic) { \
    ESP_LOGCONFIG(TAG, "  Command Topic: '%s'", this->get_command_topic_().c_str()); \
  }

#define MQTT_COMPONENT_CUSTOM_TOPIC_(name, type) \
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
 public:
  /// Constructs a MQTTComponent.
  explicit MQTTComponent();

  /// Override setup_ so that we can call send_discovery() when needed.
  void call_setup() override;

  void call_loop() override;

  void call_dump_config() override;

  /// Send discovery info the Home Assistant, override this.
  virtual void send_discovery(JsonObject root, SendDiscoveryConfig &config) = 0;

  virtual bool send_initial_state() = 0;

  virtual bool is_internal();

  /// Set whether state message should be retained.
  void set_retain(bool retain);
  bool get_retain() const;

  /// Disable discovery. Sets friendly name to "".
  void disable_discovery();
  bool is_discovery_enabled() const;

  /// Override this method to return the component type (e.g. "light", "sensor", ...)
  virtual std::string component_type() const = 0;

  /// Set a custom state topic. Set to "" for default behavior.
  void set_custom_state_topic(const std::string &custom_state_topic);
  /// Set a custom command topic. Set to "" for default behavior.
  void set_custom_command_topic(const std::string &custom_command_topic);
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

  /** Send a MQTT message.
   *
   * @param topic The topic.
   * @param payload The payload.
   */
  bool publish(const std::string &topic, const std::string &payload);

  /** Construct and send a JSON MQTT message.
   *
   * @param topic The topic.
   * @param f The Json Message builder.
   */
  bool publish_json(const std::string &topic, const json::json_build_t &f);

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
  /// Helper method to get the discovery topic for this component.
  std::string get_discovery_topic_(const MQTTDiscoveryInfo &discovery_info) const;

  /** Get this components state/command/... topic.
   *
   * @param suffix The suffix/key such as "state" or "command".
   * @return The full topic.
   */
  std::string get_default_topic_for_(const std::string &suffix) const;

  /**
   * Gets the Entity served by this MQTT component.
   */
  virtual const EntityBase *get_entity() const = 0;

  /** A unique ID for this MQTT component, empty for no unique id. See unique ID requirements:
   * https://developers.home-assistant.io/docs/en/entity_registry_index.html#unique-id-requirements
   *
   * @return The unique id as a string.
   */
  virtual std::string unique_id();

  /// Get the friendly name of this MQTT component.
  virtual std::string friendly_name() const;

  /// Get the icon field of this component
  virtual std::string get_icon() const;

  /// Get whether the underlying Entity is disabled by default
  virtual bool is_disabled_by_default() const;

  /// Get the MQTT topic that new states will be shared to.
  std::string get_state_topic_() const;

  /// Get the MQTT topic for listening to commands.
  std::string get_command_topic_() const;

  bool is_connected_() const;

  /// Internal method to start sending discovery info, this will call send_discovery().
  bool send_discovery_();

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Generate the Home Assistant MQTT discovery object id by automatically transforming the friendly name.
  std::string get_default_object_id_() const;

  std::string custom_state_topic_{};
  std::string custom_command_topic_{};
  bool command_retain_{false};
  bool retain_{true};
  bool discovery_enabled_{true};
  std::unique_ptr<Availability> availability_;
  bool resend_state_{false};
};

}  // namespace mqtt
}  // namespace esphome

#endif  // USE_MQTt
