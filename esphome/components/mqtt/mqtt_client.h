#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT

#include "esphome/components/json/json_util.h"
#include "esphome/components/network/ip_address.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include "esphome/core/string_ref.h"
#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif
#if defined(USE_ESP32)
#include "mqtt_backend_esp32.h"
#elif defined(USE_ESP8266)
#include "mqtt_backend_esp8266.h"
#elif defined(USE_LIBRETINY)
#include "mqtt_backend_libretiny.h"
#endif
#if defined(USE_MQTT_SESSION_PERSISTENCE) && !defined(USE_ESP32)
#include "mqtt_subscription_fingerprint.h"
#endif
#include "lwip/ip_addr.h"

#include <span>
#include <vector>

// The crash log replay needs the logger, which mqtt does not depend on
#if defined(USE_LOGGER) && defined(USE_CRASH_HANDLER)
#define USE_MQTT_CRASH_LOG
#endif

namespace esphome::mqtt {

// Max lengths for stack-based topic building.
// These limits are enforced at Python config validation time in mqtt/__init__.py
// using cv.Length() validators for topic_prefix.
// This ensures the stack buffers are always large enough.
static constexpr size_t MQTT_COMPONENT_TYPE_MAX_LEN = 20;
static constexpr size_t MQTT_SUFFIX_MAX_LEN = 32;
static constexpr size_t MQTT_TOPIC_PREFIX_MAX_LEN = 64;  // Validated in Python: cv.Length(max=64)
#if defined(USE_MQTT_SUB_DEVICE_TOPICS) && defined(USE_DEVICES)
// Longest sanitized sub-device name in the config (set by codegen) plus its separator
static_assert(MQTT_SUB_DEVICE_NAME_MAX_LEN <= ESPHOME_FRIENDLY_NAME_MAX_LEN, "sub-device name too long");
static constexpr size_t MQTT_SUB_DEVICE_SEGMENT_MAX_LEN = MQTT_SUB_DEVICE_NAME_MAX_LEN + 1;
#else
static constexpr size_t MQTT_SUB_DEVICE_SEGMENT_MAX_LEN = 0;
#endif
// Stack buffer size - safe because all inputs are length-validated at config time
// Format: prefix + "/" + [sub_device + "/"] + type + "/" + object_id + "/" + suffix + null
static constexpr size_t MQTT_DEFAULT_TOPIC_MAX_LEN = MQTT_TOPIC_PREFIX_MAX_LEN + 1 + MQTT_SUB_DEVICE_SEGMENT_MAX_LEN +
                                                     MQTT_COMPONENT_TYPE_MAX_LEN + 1 + OBJECT_ID_MAX_LEN + 1 +
                                                     MQTT_SUFFIX_MAX_LEN + 1;

/** Callback for MQTT events.
 */
using mqtt_on_connect_callback_t = std::function<MQTTBackend::on_connect_callback_t>;
using mqtt_on_disconnect_callback_t = std::function<MQTTBackend::on_disconnect_callback_t>;

/// Stored callback of an MQTT subscription: the topic, then the payload.
using mqtt_subscription_callback_t = Callback<void(const std::string &, const std::string &)>;

// Remove before 2027.5.0
using mqtt_callback_t ESPDEPRECATED("subscribe() takes any callable; pass the lambda directly. Removed in 2027.5.0",
                                    "2026.11.0") = std::function<void(const std::string &, const std::string &)>;
using mqtt_json_callback_t ESPDEPRECATED(
    "subscribe_json() takes any callable; pass the lambda directly. Removed in 2027.5.0",
    "2026.11.0") = std::function<void(const std::string &, JsonObject)>;

/// Returns a subscription's topic, written into buf or referring to storage that outlives the subscription.
/// Never a null c_str(); an empty topic is "".
using mqtt_topic_builder_t = StringRef (*)(const void *context, std::span<char, MQTT_DEFAULT_TOPIC_MAX_LEN> buf);

/// Where a subscription's topic comes from. The topic is rebuilt whenever it is needed, so nothing is stored.
struct MQTTTopicSource {
  mqtt_topic_builder_t builder;
  const void *context;
  StringRef build(std::span<char, MQTT_DEFAULT_TOPIC_MAX_LEN> buf) const { return this->builder(this->context, buf); }
};

/// A topic string that outlives the subscription, such as a literal; it is referenced, not copied.
MQTTTopicSource static_topic(const char *topic);
#ifdef USE_ESP8266
/// A topic in flash, copied into the buffer when needed, so at most MQTT_DEFAULT_TOPIC_MAX_LEN - 1 characters.
MQTTTopicSource progmem_topic(ProgmemStr topic);
#endif

/// internal struct for MQTT subscriptions.
struct MQTTSubscription {
  MQTTTopicSource topic;
  mqtt_subscription_callback_t callback;
  uint32_t resubscribe_timeout;
  uint8_t qos;
  bool subscribed;
  bool boxed;    // made by create_boxed(), freed on unsubscribe
  bool removed;  // unsubscribed while a callback ran, erased once dispatch returns
};

/// internal struct for MQTT credentials.
struct MQTTCredentials {
  std::string address;  ///< The address of the server without port number
  uint16_t port;        ///< The port number of the server.
  std::string username;
  std::string password;
  std::string client_id;  ///< The client ID. Will automatically be truncated to 23 characters.
  bool clean_session;     ///< Whether the session will be cleaned or remembered between connects.
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
  MQTT_CLIENT_DISABLED = 0,
  MQTT_CLIENT_DISCONNECTED,
  MQTT_CLIENT_RESOLVING_ADDRESS,
  MQTT_CLIENT_CONNECTING,
  MQTT_CLIENT_CONNECTED,
};

class MQTTComponent;

class MQTTClientComponent final : public Component {
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

#if defined(USE_ESP32) && defined(USE_MQTT_TLS)
  void set_ca_certificate(const char *cert) { this->mqtt_backend_.set_ca_certificate(cert); }
  void set_cl_certificate(const char *cert) { this->mqtt_backend_.set_cl_certificate(cert); }
  void set_cl_key(const char *key) { this->mqtt_backend_.set_cl_key(key); }
  void set_skip_cert_cn_check(bool skip_check) { this->mqtt_backend_.set_skip_cert_cn_check(skip_check); }
#endif
#ifdef USE_ESP32
  void set_transport(MQTTTransport transport) { this->mqtt_backend_.set_transport(transport); }
  void set_ws_path(const char *path) { this->mqtt_backend_.set_ws_path(path); }
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
  void set_topic_prefix(std::string topic_prefix);
  /// Get the topic prefix of this device, using default if necessary
  const std::string &get_topic_prefix() const;

  /// Manually set the topic used for logging.
  void set_log_message_template(MQTTMessage &&message);
  void set_log_level(int level) { this->log_level_ = level; }
  /// Get the topic used for logging. Defaults to "<topic_prefix>/debug" and the value is cached for speed.
  void disable_log_message();
  bool is_log_message_enabled() const;

  /** Subscribe to an MQTT topic and call callback when a message is received.
   *
   * A callback that fits in a pointer (capture at most `this`) is stored inline; a larger one is
   * boxed on the heap and freed by unsubscribe().
   *
   * @param topic Where the topic comes from; it is rebuilt when needed instead of stored.
   * @param callback The callback function.
   * @param qos The QoS of this subscription.
   */
  template<typename F> void subscribe(MQTTTopicSource topic, F &&callback, uint8_t qos = 0) {
    constexpr bool boxed = !mqtt_subscription_callback_t::fits_inline<F>();
    this->add_subscription_(topic,
                            boxed ? mqtt_subscription_callback_t::create_boxed(std::forward<F>(callback))
                                  : mqtt_subscription_callback_t::create(std::forward<F>(callback)),
                            boxed, qos);
  }

  /** Subscribe to an MQTT topic and call callback when a message is received.
   *
   * The topic is copied to the heap and freed by unsubscribe().
   *
   * @param topic The topic. Wildcards are currently not supported.
   * @param callback The callback function.
   * @param qos The QoS of this subscription.
   */
  template<typename F> void subscribe(const std::string &topic, F &&callback, uint8_t qos = 0) {
    this->subscribe(copy_topic(StringRef(topic)), std::forward<F>(callback), qos);
  }

  /** Subscribe to a MQTT topic and automatically parse JSON payload.
   *
   * If an invalid JSON payload is received, the callback will not be called.
   *
   * @param topic An MQTTTopicSource, or a string that is copied to the heap.
   * @param callback The callback with a parsed JsonObject that will be called when a message with matching topic is
   * received.
   * @param qos The QoS of this subscription.
   */
  template<typename T, typename F> void subscribe_json(T &&topic, F &&callback, uint8_t qos = 0) {
    static_assert(std::is_invocable_v<const std::decay_t<F> &, const std::string &, JsonObject>,
                  "subscribe_json() needs a callback that does not mutate its captures");
    this->subscribe(
        std::forward<T>(topic),
        [cb = std::forward<F>(callback)](const std::string &topic, const std::string &payload) {
          call_json(cb, topic, payload);
        },
        qos);
  }

  /** Unsubscribe from an MQTT topic.
   *
   * If multiple existing subscriptions to the same topic exist, all of them will be removed.
   * From inside a subscription callback they stop receiving messages at once and are erased once it returns.
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

  /// Publish directly without creating MQTTMessage (avoids heap allocation for topic)
  bool publish(const char *topic, const char *payload, size_t payload_length, uint8_t qos = 0, bool retain = false);

  /** Construct and send a JSON MQTT message.
   *
   * @param topic The topic.
   * @param f The Json Message builder.
   * @param retain Whether to retain the message.
   */
  bool publish_json(const std::string &topic, const json::json_build_t &f, uint8_t qos = 0, bool retain = false);

  /// Publish JSON directly without heap allocation for topic
  bool publish_json(const char *topic, const json::json_build_t &f, uint8_t qos = 0, bool retain = false);

  /// Setup the MQTT client, registering a bunch of callbacks and attempting to connect.
  void setup() override;
  void dump_config() override;
  /// Reconnect if required
  void loop() override;
  /// MQTT client setup priority
  float get_setup_priority() const override;

#ifdef USE_LOGGER
  void on_log(uint8_t level, const char *tag, const char *message, size_t message_len);
#endif

  void on_message(const std::string &topic, const std::string &payload);

  bool can_proceed() override;

  void check_connected();

  void set_reboot_timeout(uint32_t reboot_timeout) { this->reboot_timeout_ = reboot_timeout; }

  void register_mqtt_component(MQTTComponent *component);

  bool is_connected();
  void set_enable_on_boot(bool enable_on_boot) { this->enable_on_boot_ = enable_on_boot; }
  void enable();
  void disable();

  void on_shutdown() override;
  bool teardown() override;

  void set_broker_address(const std::string &address) { this->credentials_.address = address; }
  void set_broker_port(uint16_t port) { this->credentials_.port = port; }
  void set_username(const std::string &username) { this->credentials_.username = username; }
  void set_password(const std::string &password) { this->credentials_.password = password; }
  void set_client_id(const std::string &client_id) { this->credentials_.client_id = client_id; }
  void set_clean_session(const bool &clean_session) { this->credentials_.clean_session = clean_session; }
  void set_on_connect(mqtt_on_connect_callback_t &&callback);
  void set_on_disconnect(mqtt_on_disconnect_callback_t &&callback);

  // Publish None state instead of NaN for Home Assistant
  void set_publish_nan_as_none(bool publish_nan_as_none) { this->publish_nan_as_none_ = publish_nan_as_none; }
  bool is_publish_nan_as_none() const { return this->publish_nan_as_none_; }

  void set_wait_for_connection(bool wait_for_connection) { this->wait_for_connection_ = wait_for_connection; }

 protected:
  // For the lambda command topic, which is copied once
  friend class MQTTComponent;

  /// A heap copy of topic, owned by the one subscription it is passed to and freed when that is erased.
  static MQTTTopicSource copy_topic(StringRef topic);

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
  bool subscribe_topic_(const MQTTTopicSource &topic, uint8_t qos);
  void resubscribe_subscription_(MQTTSubscription *sub);
  template<typename F> static void call_json(const F &callback, const std::string &topic, const std::string &payload) {
    JsonDocument doc = json::parse_json(payload);
    if (doc.overflowed() || doc.isNull())
      return;
    callback(topic, doc.as<JsonObject>());
  }
  void erase_removed_subscriptions_();
#ifdef USE_MQTT_CRASH_LOG
  void subscribe_crash_log_();
  void handle_crash_log_request_(const std::string &topic, const std::string &payload);
#endif
  void add_subscription_(MQTTTopicSource topic, mqtt_subscription_callback_t callback, bool boxed, uint8_t qos);
  /// Returns true when every subscription is subscribed
  bool resubscribe_subscriptions_();
#ifdef USE_MQTT_SESSION_PERSISTENCE
  uint32_t subscriptions_fingerprint_();
  /// Once every SUBACK of this connection is in, saves the fingerprint if all were granted
  void check_subacks_();
  void set_session_fingerprint_(uint32_t fingerprint);
#endif

  MQTTCredentials credentials_;
  /// The last will message. Disabled optional denotes it being default and
  /// an empty topic denotes the the feature being disabled.
  MQTTMessage last_will_;
  /// The birth message (e.g. the message that's send on an established connection.
  /// See last_will_ for what different values denote.
  MQTTMessage birth_message_;
  bool sent_birth_message_{false};
  bool shutdown_message_sent_{false};
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

  MQTTClientState state_{MQTT_CLIENT_DISABLED};
  network::IPAddress ip_;
  bool dns_resolved_{false};
  bool dns_resolve_error_{false};
  bool enable_on_boot_{true};
#ifdef USE_MQTT_CRASH_LOG
  bool crash_log_offered_{false};
#endif
  std::vector<MQTTComponent *> children_;
  uint32_t reboot_timeout_{300000};
  uint32_t connect_begin_;
  uint32_t last_connected_{0};
  optional<MQTTClientDisconnectReason> disconnect_reason_{};
  CallbackManager<MQTTBackend::on_disconnect_callback_t> on_disconnect_;

  bool publish_nan_as_none_{false};
  bool wait_for_connection_{false};
  bool dispatching_{false};
  bool removed_while_dispatching_{false};
#ifdef USE_MQTT_SESSION_PERSISTENCE
  bool session_present_{false};
  /// Set once every SUBACK of this connection is in (or the session was resumed without subscribing)
  bool subacks_settled_{false};
  /// SUBSCRIBEs sent on this connection for the subscriptions the fingerprint covers
  uint16_t subscribes_sent_{0};
#ifndef USE_ESP32
  SubackCounter subacks_;
#endif
  /// Fingerprint of the set the broker session is known to hold in full; 0 while a rebuild is in progress
  uint32_t session_fingerprint_{0};
#endif
};

extern MQTTClientComponent *global_mqtt_client;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

template<typename... Ts> class MQTTPublishJsonAction final : public Action<Ts...> {
 public:
  MQTTPublishJsonAction(MQTTClientComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, topic)
  TEMPLATABLE_VALUE(uint8_t, qos)
  TEMPLATABLE_VALUE(bool, retain)

  void set_payload(std::function<void(Ts..., JsonObject)> payload) { this->payload_ = payload; }

  void play(const Ts &...x) override {
    auto topic = this->topic_.value(x...);
    auto qos = this->qos_.value(x...);
    auto retain = this->retain_.value(x...);
    this->parent_->publish_json(topic, [this, x...](JsonObject root) { this->payload_(x..., root); }, qos, retain);
  }

 protected:
  std::function<void(Ts..., JsonObject)> payload_;
  MQTTClientComponent *parent_;
};

}  // namespace esphome::mqtt

#endif  // USE_MQTT
