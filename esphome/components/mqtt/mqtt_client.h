#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/log.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/network/ip_address.h"
#if defined(USE_ESP32)
#include "mqtt_backend_esp32.h"
#elif defined(USE_ESP8266)
#include "mqtt_backend_esp8266.h"
#elif defined(USE_LIBRETINY)
#include "mqtt_backend_libretiny.h"
#endif
#include "lwip/ip_addr.h"

#include <vector>

namespace esphome {
namespace mqtt {

/** Callback for MQTT events.
 */
using mqtt_on_connect_callback_t = std::function<MQTTBackend::on_connect_callback_t>;
using mqtt_on_disconnect_callback_t = std::function<MQTTBackend::on_disconnect_callback_t>;

/** Callback for MQTT subscriptions.
 *
 * First parameter is the topic, the second one is the payload.
 */
using mqtt_callback_t = std::function<void(const std::string &, const std::string &)>;
using mqtt_json_callback_t = std::function<void(const std::string &, JsonObject)>;

/// internal struct for MQTT subscriptions.
struct MQTTSubscription {
  std::string topic;
  uint8_t qos;
  mqtt_callback_t callback;
  bool subscribed;
  uint32_t resubscribe_timeout;
};

/// internal struct for MQTT credentials.
struct MQTTCredentials {
  std::string address;  ///< The address of the server without port number
  uint16_t port;        ///< The port number of the server.
  std::string username;
  std::string password;
  std::string client_id;  ///< The client ID. Will automatically be truncated to 23 characters.
};

/// Simple data struct for Home Assistant component availability.
struct Availability {
  std::string topic;  ///< Empty means disabled
  std::string payload_available;
  std::string payload_not_available;
};

/// available discovery unique_id generators
enum MQTTDiscoveryUniqueIdGenerator {
  MQTT_LEGACY_UNIQUE_ID_GENERATOR = 0,
  MQTT_MAC_ADDRESS_UNIQUE_ID_GENERATOR,
};

/// available discovery object_id generators
enum MQTTDiscoveryObjectIdGenerator {
  MQTT_NONE_OBJECT_ID_GENERATOR = 0,
  MQTT_DEVICE_NAME_OBJECT_ID_GENERATOR,
};

/** Internal struct for MQTT Home Assistant discovery
 *
 * See <a href="https://www.home-assistant.io/docs/mqtt/discovery/">MQTT Discovery</a>.
 */
struct MQTTDiscoveryInfo {
  std::string prefix;  ///< The Home Assistant discovery prefix. Empty means disabled.
  bool retain;         ///< Whether to retain discovery messages.
  bool discover_ip;    ///< Enable the Home Assistant device discovery.
  bool clean;
  MQTTDiscoveryUniqueIdGenerator unique_id_generator;
  MQTTDiscoveryObjectIdGenerator object_id_generator;
};

enum MQTTClientState {
  MQTT_CLIENT_DISCONNECTED = 0,
  MQTT_CLIENT_RESOLVING_ADDRESS,
  MQTT_CLIENT_CONNECTING,
  MQTT_CLIENT_CONNECTED,
};

class MQTTComponent;

class MQTTClientComponent : public Component {
 public:
  MQTTClientComponent();

  /// Set the last will testament message.
  void set_last_will(MQTTMessage &&message);
  /// Remove the last will testament message.
  void disable_last_will();

  /// Set the birth message.
  void set_birth_message(MQTTMessage &&message);
  /// Remove the birth message.
  void disable_birth_message();

  void set_shutdown_message(MQTTMessage &&message);
  void disable_shutdown_message();

  /// Set the keep alive time in seconds, every 0.7*keep_alive a ping will be sent.
  void set_keep_alive(uint16_t keep_alive_s);

  /** Set the Home Assistant discovery info
   *
   * See <a href="https://www.home-assistant.io/docs/mqtt/discovery/">MQTT Discovery</a>.
   * @param prefix The Home Assistant discovery prefix.
   * @param unique_id_generator Controls how UniqueId is generated.
   * @param object_id_generator Controls how ObjectId is generated.
   * @param retain Whether to retain discovery messages.
   */
  void set_discovery_info(std::string &&prefix, MQTTDiscoveryUniqueIdGenerator unique_id_generator,
                          MQTTDiscoveryObjectIdGenerator object_id_generator, bool retain, bool discover_ip,
                          bool clean = false);
  /// Get Home Assistant discovery info.
  const MQTTDiscoveryInfo &get_discovery_info() const;
  /// Globally disable Home Assistant discovery.
  void disable_discovery();
  bool is_discovery_enabled() const;
  bool is_discovery_ip_enabled() const;

#if ASYNC_TCP_SSL_ENABLED
  /** Add a SSL fingerprint to use for TCP SSL connections to the MQTT broker.
   *
   * To use this feature you first have to globally enable the `ASYNC_TCP_SSL_ENABLED` define flag.
   * This function can be called multiple times and any certificate that matches any of the provided fingerprints
   * will match. Calling this method will also automatically disable all non-ssl connections.
   *
   * @warning This is *not* secure and *not* how SSL is usually done. You'll have to add
   *          a separate fingerprint for every certificate you use. Additionally, the hashing
   *          algorithm used here due to the constraints of the MCU, SHA1, is known to be insecure.
   *
   * @param fingerprint The SSL fingerprint as a 20 value long std::array.
   */
  void add_ssl_fingerprint(const std::array<uint8_t, SHA1_SIZE> &fingerprint);
#endif
#ifdef USE_ESP32
  void set_ca_certificate(const char *cert) { this->mqtt_backend_.set_ca_certificate(cert); }
  void set_cl_certificate(const char *cert) { this->mqtt_backend_.set_cl_certificate(cert); }
  void set_cl_key(const char *key) { this->mqtt_backend_.set_cl_key(key); }
  void set_skip_cert_cn_check(bool skip_check) { this->mqtt_backend_.set_skip_cert_cn_check(skip_check); }
#endif
  const Availability &get_availability();

  /** Set the topic prefix that will be prepended to all topics together with "/". This will, in most cases,
   * be the name of your Application.
   *
   * For example, if "livingroom" is passed to this method, all state topics will, by default, look like
   * "livingroom/.../state"
   *
   * @param topic_prefix The topic prefix. The last "/" is appended automatically.
   */
  void set_topic_prefix(const std::string &topic_prefix);
  /// Get the topic prefix of this device, using default if necessary
  const std::string &get_topic_prefix() const;

  /// Manually set the topic used for logging.
  void set_log_message_template(MQTTMessage &&message);
  void set_log_level(int level);
  /// Get the topic used for logging. Defaults to "<topic_prefix>/debug" and the value is cached for speed.
  void disable_log_message();
  bool is_log_message_enabled() const;

  /** Subscribe to an MQTT topic and call callback when a message is received.
   *
   * @param topic The topic. Wildcards are currently not supported.
   * @param callback The callback function.
   * @param qos The QoS of this subscription.
   */
  void subscribe(const std::string &topic, mqtt_callback_t callback, uint8_t qos = 0);

  /** Subscribe to a MQTT topic and automatically parse JSON payload.
   *
   * If an invalid JSON payload is received, the callback will not be called.
   *
   * @param topic The topic. Wildcards are currently not supported.
   * @param callback The callback with a parsed JsonObject that will be called when a message with matching topic is
   * received.
   * @param qos The QoS of this subscription.
   */
  void subscribe_json(const std::string &topic, const mqtt_json_callback_t &callback, uint8_t qos = 0);

  /** Unsubscribe from an MQTT topic.
   *
   * If multiple existing subscriptions to the same topic exist, all of them will be removed.
   *
   * @param topic The topic to unsubscribe from.
   * Must match the topic in the original subscribe or subscribe_json call exactly.
   */
  void unsubscribe(const std::string &topic);

  /** Publish a MQTTMessage
   *
   * @param message The message.
   */
  bool publish(const MQTTMessage &message);

  /** Publish a MQTT message
   *
   * @param topic The topic.
   * @param payload The payload.
   * @param retain Whether to retain the message.
   */
  bool publish(const std::string &topic, const std::string &payload, uint8_t qos = 0, bool retain = false);

  bool publish(const std::string &topic, const char *payload, size_t payload_length, uint8_t qos = 0,
               bool retain = false);

  /** Construct and send a JSON MQTT message.
   *
   * @param topic The topic.
   * @param f The Json Message builder.
   * @param retain Whether to retain the message.
   */
  bool publish_json(const std::string &topic, const json::json_build_t &f, uint8_t qos = 0, bool retain = false);

  /// Setup the MQTT client, registering a bunch of callbacks and attempting to connect.
  void setup() override;
  void dump_config() override;
  /// Reconnect if required
  void loop() override;
  /// MQTT client setup priority
  float get_setup_priority() const override;

  void on_message(const std::string &topic, const std::string &payload);

  bool can_proceed() override;

  void check_connected();

  void set_reboot_timeout(uint32_t reboot_timeout);

  void register_mqtt_component(MQTTComponent *component);

  bool is_connected();

  void on_shutdown() override;

  void set_broker_address(const std::string &address) { this->credentials_.address = address; }
  void set_broker_port(uint16_t port) { this->credentials_.port = port; }
  void set_username(const std::string &username) { this->credentials_.username = username; }
  void set_password(const std::string &password) { this->credentials_.password = password; }
  void set_client_id(const std::string &client_id) { this->credentials_.client_id = client_id; }
  void set_on_connect(mqtt_on_connect_callback_t &&callback);
  void set_on_disconnect(mqtt_on_disconnect_callback_t &&callback);

 protected:
  void send_device_info_();

  /// Reconnect to the MQTT broker if not already connected.
  void start_connect_();
  void start_dnslookup_();
  void check_dnslookup_();
#if defined(USE_ESP8266) && LWIP_VERSION_MAJOR == 1
  static void dns_found_callback(const char *name, ip_addr_t *ipaddr, void *callback_arg);
#else
  static void dns_found_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg);
#endif

  /// Re-calculate the availability property.
  void recalculate_availability_();

  bool subscribe_(const char *topic, uint8_t qos);
  void resubscribe_subscription_(MQTTSubscription *sub);
  void resubscribe_subscriptions_();

  MQTTCredentials credentials_;
  /// The last will message. Disabled optional denotes it being default and
  /// an empty topic denotes the the feature being disabled.
  MQTTMessage last_will_;
  /// The birth message (e.g. the message that's send on an established connection.
  /// See last_will_ for what different values denote.
  MQTTMessage birth_message_;
  bool sent_birth_message_{false};
  MQTTMessage shutdown_message_;
  /// Caches availability.
  Availability availability_{};
  /// The discovery info options for Home Assistant. Undefined optional means
  /// default and empty prefix means disabled.
  MQTTDiscoveryInfo discovery_info_{
      .prefix = "homeassistant",
      .retain = true,
      .discover_ip = true,
      .clean = false,
      .unique_id_generator = MQTT_LEGACY_UNIQUE_ID_GENERATOR,
      .object_id_generator = MQTT_NONE_OBJECT_ID_GENERATOR,
  };
  std::string topic_prefix_{};
  MQTTMessage log_message_;
  std::string payload_buffer_;
  int log_level_{ESPHOME_LOG_LEVEL};

  std::vector<MQTTSubscription> subscriptions_;
#if defined(USE_ESP32)
  MQTTBackendESP32 mqtt_backend_;
#elif defined(USE_ESP8266)
  MQTTBackendESP8266 mqtt_backend_;
#elif defined(USE_LIBRETINY)
  MQTTBackendLibreTiny mqtt_backend_;
#endif

  MQTTClientState state_{MQTT_CLIENT_DISCONNECTED};
  network::IPAddress ip_;
  bool dns_resolved_{false};
  bool dns_resolve_error_{false};
  std::vector<MQTTComponent *> children_;
  uint32_t reboot_timeout_{300000};
  uint32_t connect_begin_;
  uint32_t last_connected_{0};
  optional<MQTTClientDisconnectReason> disconnect_reason_{};
};

extern MQTTClientComponent *global_mqtt_client;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

class MQTTMessageTrigger : public Trigger<std::string>, public Component {
 public:
  explicit MQTTMessageTrigger(std::string topic);

  void set_qos(uint8_t qos);
  void set_payload(const std::string &payload);
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

 protected:
  std::string topic_;
  uint8_t qos_{0};
  optional<std::string> payload_;
};

class MQTTJsonMessageTrigger : public Trigger<JsonObjectConst> {
 public:
  explicit MQTTJsonMessageTrigger(const std::string &topic, uint8_t qos) {
    global_mqtt_client->subscribe_json(
        topic, [this](const std::string &topic, JsonObject root) { this->trigger(root); }, qos);
  }
};

class MQTTConnectTrigger : public Trigger<> {
 public:
  explicit MQTTConnectTrigger(MQTTClientComponent *&client) {
    client->set_on_connect([this](bool session_present) { this->trigger(); });
  }
};

class MQTTDisconnectTrigger : public Trigger<> {
 public:
  explicit MQTTDisconnectTrigger(MQTTClientComponent *&client) {
    client->set_on_disconnect([this](MQTTClientDisconnectReason reason) { this->trigger(); });
  }
};

template<typename... Ts> class MQTTPublishAction : public Action<Ts...> {
 public:
  MQTTPublishAction(MQTTClientComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, topic)
  TEMPLATABLE_VALUE(std::string, payload)
  TEMPLATABLE_VALUE(uint8_t, qos)
  TEMPLATABLE_VALUE(bool, retain)

  void play(Ts... x) override {
    this->parent_->publish(this->topic_.value(x...), this->payload_.value(x...), this->qos_.value(x...),
                           this->retain_.value(x...));
  }

 protected:
  MQTTClientComponent *parent_;
};

template<typename... Ts> class MQTTPublishJsonAction : public Action<Ts...> {
 public:
  MQTTPublishJsonAction(MQTTClientComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, topic)
  TEMPLATABLE_VALUE(uint8_t, qos)
  TEMPLATABLE_VALUE(bool, retain)

  void set_payload(std::function<void(Ts..., JsonObject)> payload) { this->payload_ = payload; }

  void play(Ts... x) override {
    auto f = std::bind(&MQTTPublishJsonAction<Ts...>::encode_, this, x..., std::placeholders::_1);
    auto topic = this->topic_.value(x...);
    auto qos = this->qos_.value(x...);
    auto retain = this->retain_.value(x...);
    this->parent_->publish_json(topic, f, qos, retain);
  }

 protected:
  void encode_(Ts... x, JsonObject root) { this->payload_(x..., root); }
  std::function<void(Ts..., JsonObject)> payload_;
  MQTTClientComponent *parent_;
};

template<typename... Ts> class MQTTConnectedCondition : public Condition<Ts...> {
 public:
  MQTTConnectedCondition(MQTTClientComponent *parent) : parent_(parent) {}
  bool check(Ts... x) override { return this->parent_->is_connected(); }

 protected:
  MQTTClientComponent *parent_;
};

}  // namespace mqtt
}  // namespace esphome

#endif  // USE_MQTT
