#include "mqtt_client.h"

#ifdef USE_MQTT

#include <utility>
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include "esphome/core/version.h"
#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif
#include "lwip/dns.h"
#include "lwip/err.h"
#include "mqtt_component.h"

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif
#ifdef USE_DASHBOARD_IMPORT
#include "esphome/components/dashboard_import/dashboard_import.h"
#endif

namespace esphome::mqtt {

static const char *const TAG = "mqtt";

// Disconnect reason strings indexed by MQTTClientDisconnectReason enum (0-8)
PROGMEM_STRING_TABLE(MQTTDisconnectReasonStrings, "TCP disconnected", "Unacceptable Protocol Version",
                     "Identifier Rejected", "Server Unavailable", "Malformed Credentials", "Not Authorized",
                     "Not Enough Space", "TLS Bad Fingerprint", "DNS Resolve Error", "Unknown");

MQTTClientComponent::MQTTClientComponent() {
  global_mqtt_client = this;
  char mac_addr[MAC_ADDRESS_BUFFER_SIZE];
  get_mac_address_into_buffer(mac_addr);
  this->credentials_.client_id = make_name_with_suffix(App.get_name(), '-', mac_addr, MAC_ADDRESS_BUFFER_SIZE - 1);
}

// Connection
void MQTTClientComponent::setup() {
  this->mqtt_backend_.set_on_message(
      [this](const char *topic, const char *payload, size_t len, size_t index, size_t total) {
        if (index == 0)
          this->payload_buffer_.reserve(total);

        // append new payload, may contain incomplete MQTT message
        this->payload_buffer_.append(payload, len);

        // MQTT fully received
        if (len + index == total) {
          this->on_message(topic, this->payload_buffer_);
          this->payload_buffer_.clear();
        }
      });
  this->mqtt_backend_.set_on_disconnect([this](MQTTClientDisconnectReason reason) {
    if (this->state_ == MQTT_CLIENT_DISABLED)
      return;
    this->state_ = MQTT_CLIENT_DISCONNECTED;
    this->disconnect_reason_ = reason;
  });
#ifdef USE_LOGGER
  if (this->is_log_message_enabled() && logger::global_logger != nullptr) {
    logger::global_logger->add_log_listener(this);
  }
#endif

  if (this->is_discovery_ip_enabled()) {
    this->subscribe(
        "esphome/discover", [this](const std::string &topic, const std::string &payload) { this->send_device_info_(); },
        2);

    // Format topic on stack - subscribe() copies it
    // "esphome/ping/" (13) + name (ESPHOME_DEVICE_NAME_MAX_LEN) + null (1)
    constexpr size_t ping_topic_buffer_size = 13 + ESPHOME_DEVICE_NAME_MAX_LEN + 1;
    char ping_topic[ping_topic_buffer_size];
    buf_append_printf(ping_topic, sizeof(ping_topic), 0, "esphome/ping/%s", App.get_name().c_str());
    this->subscribe(
        ping_topic, [this](const std::string &topic, const std::string &payload) { this->send_device_info_(); }, 2);
  }

  if (this->enable_on_boot_) {
    this->enable();
  }
}

void MQTTClientComponent::send_device_info_() {
  if (!this->is_connected() or !this->is_discovery_ip_enabled()) {
    return;
  }
  // Format topic on stack to avoid heap allocation
  // "esphome/discover/" (17) + name (ESPHOME_DEVICE_NAME_MAX_LEN) + null (1)
  constexpr size_t topic_buffer_size = 17 + ESPHOME_DEVICE_NAME_MAX_LEN + 1;
  char topic[topic_buffer_size];
  buf_append_printf(topic, sizeof(topic), 0, "esphome/discover/%s", App.get_name().c_str());

  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  this->publish_json(
      topic,
      [](JsonObject root) {
        uint8_t index = 0;
        for (auto &ip : network::get_ip_addresses()) {
          if (ip.is_set()) {
            char key[8];  // "ip" + up to 3 digits + null
            char ip_buf[network::IP_ADDRESS_BUFFER_SIZE];
            if (index == 0) {
              key[0] = 'i';
              key[1] = 'p';
              key[2] = '\0';
            } else {
              buf_append_printf(key, sizeof(key), 0, "ip%u", index);
            }
            ip.str_to(ip_buf);
            root[key] = ip_buf;
            index++;
          }
        }
        root[ESPHOME_F("name")] = App.get_name();
        if (!App.get_friendly_name().empty()) {
          root[ESPHOME_F("friendly_name")] = App.get_friendly_name();
        }
#ifdef USE_API
        root[ESPHOME_F("port")] = api::global_api_server->get_port();
#endif
        root[ESPHOME_F("version")] = ESPHOME_VERSION;
        char mac_buf[MAC_ADDRESS_BUFFER_SIZE];
        get_mac_address_into_buffer(mac_buf);
        root[ESPHOME_F("mac")] = mac_buf;

#ifdef USE_ESP8266
        root[ESPHOME_F("platform")] = ESPHOME_F("ESP8266");
#endif
#ifdef USE_ESP32
        root[ESPHOME_F("platform")] = ESPHOME_F("ESP32");
#endif
#ifdef USE_LIBRETINY
        root[ESPHOME_F("platform")] = lt_cpu_get_model_name();
#endif

        root[ESPHOME_F("board")] = ESPHOME_BOARD;
#if defined(USE_WIFI)
        root[ESPHOME_F("network")] = ESPHOME_F("wifi");
#elif defined(USE_ETHERNET)
        root[ESPHOME_F("network")] = ESPHOME_F("ethernet");
#endif

#ifdef ESPHOME_PROJECT_NAME
        root[ESPHOME_F("project_name")] = ESPHOME_PROJECT_NAME;
        root[ESPHOME_F("project_version")] = ESPHOME_PROJECT_VERSION;
#endif  // ESPHOME_PROJECT_NAME

#ifdef USE_DASHBOARD_IMPORT
        root[ESPHOME_F("package_import_url")] = dashboard_import::get_package_import_url();
#endif

#ifdef USE_API_NOISE
        root[api::global_api_server->get_noise_ctx().has_psk() ? ESPHOME_F("api_encryption")
                                                               : ESPHOME_F("api_encryption_supported")] =
            ESPHOME_F("Noise_NNpsk0_25519_ChaChaPoly_SHA256");
#endif
      },
      2, this->discovery_info_.retain);
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

#ifdef USE_LOGGER
void MQTTClientComponent::on_log(uint8_t level, const char *tag, const char *message, size_t message_len) {
  (void) tag;
  if (level <= this->log_level_ && this->is_connected()) {
    this->publish(this->log_message_.topic.c_str(), message, message_len, this->log_message_.qos,
                  this->log_message_.retain);
  }
}
#endif

void MQTTClientComponent::dump_config() {
  char ip_buf[network::IP_ADDRESS_BUFFER_SIZE];
  // clang-format off
  ESP_LOGCONFIG(TAG,
                "MQTT:\n"
                "  Server Address: %s:%u (%s)\n"
                "  Username: " LOG_SECRET("'%s'") "\n"
                "  Client ID: " LOG_SECRET("'%s'") "\n"
                "  Clean Session: %s",
                this->credentials_.address.c_str(), this->credentials_.port, this->ip_.str_to(ip_buf),
                this->credentials_.username.c_str(), this->credentials_.client_id.c_str(),
                YESNO(this->credentials_.clean_session));
  // clang-format on
  if (this->is_discovery_ip_enabled()) {
    ESP_LOGCONFIG(TAG, "  Discovery IP enabled");
  }
  if (!this->discovery_info_.prefix.empty()) {
    ESP_LOGCONFIG(TAG,
                  "  Discovery prefix: '%s'\n"
                  "  Discovery retain: %s",
                  this->discovery_info_.prefix.c_str(), YESNO(this->discovery_info_.retain));
  }
  ESP_LOGCONFIG(TAG, "  Topic Prefix: '%s'", this->topic_prefix_.c_str());
  if (!this->log_message_.topic.empty()) {
    ESP_LOGCONFIG(TAG, "  Log Topic: '%s'", this->log_message_.topic.c_str());
  }
  if (!this->availability_.topic.empty()) {
    ESP_LOGCONFIG(TAG, "  Availability: '%s'", this->availability_.topic.c_str());
  }
}
bool MQTTClientComponent::can_proceed() {
  return network::is_disabled() || this->state_ == MQTT_CLIENT_DISABLED || this->is_connected() ||
         !this->wait_for_connection_;
}

void MQTTClientComponent::start_dnslookup_() {
  for (auto &subscription : this->subscriptions_) {
    subscription.subscribed = false;
    subscription.resubscribe_timeout = 0;
  }

  this->status_set_warning();
  this->dns_resolve_error_ = false;
  this->dns_resolved_ = false;
  ip_addr_t addr;
  err_t err;
  {
    LwIPLock lock;
#if USE_NETWORK_IPV6
    err = dns_gethostbyname_addrtype(this->credentials_.address.c_str(), &addr, MQTTClientComponent::dns_found_callback,
                                     this, LWIP_DNS_ADDRTYPE_IPV6_IPV4);
#else
    err = dns_gethostbyname_addrtype(this->credentials_.address.c_str(), &addr, MQTTClientComponent::dns_found_callback,
                                     this, LWIP_DNS_ADDRTYPE_IPV4);
#endif /* USE_NETWORK_IPV6 */
  }
  switch (err) {
    case ERR_OK: {
      // Got IP immediately
      this->dns_resolved_ = true;
      this->ip_ = network::IPAddress(&addr);
      this->start_connect_();
      return;
    }
    case ERR_INPROGRESS: {
      // wait for callback
      ESP_LOGD(TAG, "Resolving broker IP address");
      break;
    }
    default:
    case ERR_ARG: {
      // error
      ESP_LOGW(TAG, "Error resolving broker IP address: %d", err);
      break;
    }
  }

  this->state_ = MQTT_CLIENT_RESOLVING_ADDRESS;
  this->connect_begin_ = millis();
}
void MQTTClientComponent::check_dnslookup_() {
  if (!this->dns_resolved_ && millis() - this->connect_begin_ > 20000) {
    this->dns_resolve_error_ = true;
  }

  if (this->dns_resolve_error_) {
    ESP_LOGW(TAG, "Couldn't resolve IP address for '%s'", this->credentials_.address.c_str());
    this->state_ = MQTT_CLIENT_DISCONNECTED;
    this->disconnect_reason_ = MQTTClientDisconnectReason::DNS_RESOLVE_ERROR;
    this->on_disconnect_.call(MQTTClientDisconnectReason::DNS_RESOLVE_ERROR);
    return;
  }

  if (!this->dns_resolved_) {
    return;
  }

  char ip_buf[network::IP_ADDRESS_BUFFER_SIZE];
  ESP_LOGD(TAG, "Resolved broker IP address to %s", this->ip_.str_to(ip_buf));
  this->start_connect_();
}
#if defined(USE_ESP8266) && LWIP_VERSION_MAJOR == 1
void MQTTClientComponent::dns_found_callback(const char *name, ip_addr_t *ipaddr, void *callback_arg) {
#else
void MQTTClientComponent::dns_found_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
#endif
  auto *a_this = (MQTTClientComponent *) callback_arg;
  if (ipaddr == nullptr) {
    a_this->dns_resolve_error_ = true;
  } else {
    a_this->ip_ = network::IPAddress(ipaddr);
    a_this->dns_resolved_ = true;
  }
}

void MQTTClientComponent::start_connect_() {
  if (!network::is_connected())
    return;

  ESP_LOGI(TAG, "Connecting");
  // Force disconnect first
  this->mqtt_backend_.disconnect();

  this->mqtt_backend_.set_client_id(this->credentials_.client_id.c_str());
  this->mqtt_backend_.set_clean_session(this->credentials_.clean_session);
  const char *username = nullptr;
  if (!this->credentials_.username.empty())
    username = this->credentials_.username.c_str();
  const char *password = nullptr;
  if (!this->credentials_.password.empty())
    password = this->credentials_.password.c_str();

  this->mqtt_backend_.set_credentials(username, password);

  this->mqtt_backend_.set_server(this->credentials_.address.c_str(), this->credentials_.port);
  if (!this->last_will_.topic.empty()) {
    this->mqtt_backend_.set_will(this->last_will_.topic.c_str(), this->last_will_.qos, this->last_will_.retain,
                                 this->last_will_.payload.c_str());
  }

  this->mqtt_backend_.connect();
  this->state_ = MQTT_CLIENT_CONNECTING;
  this->connect_begin_ = millis();
}
bool MQTTClientComponent::is_connected() {
  return this->state_ == MQTT_CLIENT_CONNECTED && this->mqtt_backend_.connected();
}

void MQTTClientComponent::check_connected() {
  if (!this->mqtt_backend_.connected()) {
    if (millis() - this->connect_begin_ > 60000) {
      this->state_ = MQTT_CLIENT_DISCONNECTED;
      this->start_dnslookup_();
    }
    return;
  }

  this->state_ = MQTT_CLIENT_CONNECTED;
  this->sent_birth_message_ = false;
  this->status_clear_warning();
  ESP_LOGI(TAG, "Connected");
  // MQTT Client needs some time to be fully set up.
  delay(100);  // NOLINT

  this->resubscribe_subscriptions_();
  this->send_device_info_();

  for (MQTTComponent *component : this->children_)
    component->schedule_resend_state();
}

void MQTTClientComponent::loop() {
  // Call the backend loop first
  mqtt_backend_.loop();

  if (this->disconnect_reason_.has_value()) {
    const LogString *reason_s = MQTTDisconnectReasonStrings::get_log_str(
        static_cast<uint8_t>(*this->disconnect_reason_), MQTTDisconnectReasonStrings::LAST_INDEX);
    if (!network::is_connected()) {
      reason_s = LOG_STR("WiFi disconnected");
    }
    ESP_LOGW(TAG, "Disconnected: %s", LOG_STR_ARG(reason_s));
    this->disconnect_reason_.reset();
  }

  const uint32_t now = App.get_loop_component_start_time();

  switch (this->state_) {
    case MQTT_CLIENT_DISABLED:
      return;  // Return to avoid a reboot when disabled
    case MQTT_CLIENT_DISCONNECTED:
      if (now - this->connect_begin_ > 5000) {
        this->start_dnslookup_();
      }
      break;
    case MQTT_CLIENT_RESOLVING_ADDRESS:
      this->check_dnslookup_();
      break;
    case MQTT_CLIENT_CONNECTING:
      this->check_connected();
      break;
    case MQTT_CLIENT_CONNECTED:
      if (!this->mqtt_backend_.connected()) {
        this->state_ = MQTT_CLIENT_DISCONNECTED;
        ESP_LOGW(TAG, "Lost client connection");
        this->start_dnslookup_();
      } else {
        if (!this->birth_message_.topic.empty() && !this->sent_birth_message_) {
          this->sent_birth_message_ = this->publish(this->birth_message_);
        }

        this->last_connected_ = now;
        this->resubscribe_subscriptions_();

        // Process pending resends for all MQTT components centrally
        // This is more efficient than each component polling in its own loop
        for (MQTTComponent *component : this->children_) {
          component->process_resend();
        }
      }
      break;
  }

  if (millis() - this->last_connected_ > this->reboot_timeout_ && this->reboot_timeout_ != 0) {
    ESP_LOGE(TAG, "Can't connect; restarting");
    App.reboot();
  }
}
float MQTTClientComponent::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

// Subscribe
bool MQTTClientComponent::subscribe_(const char *topic, uint8_t qos) {
  if (!this->is_connected())
    return false;

  bool ret = this->mqtt_backend_.subscribe(topic, qos);
  yield();

  if (ret) {
    ESP_LOGV(TAG, "subscribe(topic='%s')", topic);
  } else {
    delay(5);
    ESP_LOGV(TAG, "Subscribe failed for topic='%s'. Will retry", topic);
    this->status_momentary_warning("subscribe", 1000);
  }
  return ret != 0;
}
void MQTTClientComponent::resubscribe_subscription_(MQTTSubscription *sub) {
  if (sub->subscribed)
    return;

  const uint32_t now = millis();
  bool do_resub = sub->resubscribe_timeout == 0 || now - sub->resubscribe_timeout > 1000;

  if (do_resub) {
    sub->subscribed = this->subscribe_(sub->topic.c_str(), sub->qos);
    sub->resubscribe_timeout = now;
  }
}
void MQTTClientComponent::resubscribe_subscriptions_() {
  for (auto &subscription : this->subscriptions_) {
    this->resubscribe_subscription_(&subscription);
  }
}

void MQTTClientComponent::subscribe(const std::string &topic, mqtt_callback_t callback, uint8_t qos) {
  MQTTSubscription subscription{
      .topic = topic,
      .qos = qos,
      .callback = std::move(callback),
      .subscribed = false,
      .resubscribe_timeout = 0,
  };
  this->resubscribe_subscription_(&subscription);
  this->subscriptions_.push_back(subscription);
}

void MQTTClientComponent::subscribe_json(const std::string &topic, const mqtt_json_callback_t &callback, uint8_t qos) {
  auto f = [callback](const std::string &topic, const std::string &payload) {
    json::parse_json(payload, [topic, callback](JsonObject root) -> bool {
      callback(topic, root);
      return true;
    });
  };
  MQTTSubscription subscription{
      .topic = topic,
      .qos = qos,
      .callback = f,
      .subscribed = false,
      .resubscribe_timeout = 0,
  };
  this->resubscribe_subscription_(&subscription);
  this->subscriptions_.push_back(subscription);
}

void MQTTClientComponent::unsubscribe(const std::string &topic) {
  bool ret = this->mqtt_backend_.unsubscribe(topic.c_str());
  yield();
  if (ret) {
    ESP_LOGV(TAG, "unsubscribe(topic='%s')", topic.c_str());
  } else {
    delay(5);
    ESP_LOGV(TAG, "Unsubscribe failed for topic='%s'.", topic.c_str());
    this->status_momentary_warning("unsubscribe", 1000);
  }

  auto it = subscriptions_.begin();
  while (it != subscriptions_.end()) {
    if (it->topic == topic) {
      it = subscriptions_.erase(it);
    } else {
      ++it;
    }
  }
}

// Publish
bool MQTTClientComponent::publish(const std::string &topic, const std::string &payload, uint8_t qos, bool retain) {
  return this->publish(topic, payload.data(), payload.size(), qos, retain);
}

bool MQTTClientComponent::publish(const std::string &topic, const char *payload, size_t payload_length, uint8_t qos,
                                  bool retain) {
  return this->publish(topic.c_str(), payload, payload_length, qos, retain);
}

bool MQTTClientComponent::publish(const MQTTMessage &message) {
  return this->publish(message.topic.c_str(), message.payload.c_str(), message.payload.length(), message.qos,
                       message.retain);
}
bool MQTTClientComponent::publish_json(const std::string &topic, const json::json_build_t &f, uint8_t qos,
                                       bool retain) {
  return this->publish_json(topic.c_str(), f, qos, retain);
}

bool MQTTClientComponent::publish(const char *topic, const char *payload, size_t payload_length, uint8_t qos,
                                  bool retain) {
  if (!this->is_connected()) {
    return false;
  }
  size_t topic_len = strlen(topic);
  bool logging_topic = (topic_len == this->log_message_.topic.size()) &&
                       (memcmp(this->log_message_.topic.c_str(), topic, topic_len) == 0);
  bool ret = this->mqtt_backend_.publish(topic, payload, payload_length, qos, retain);
  delay(0);
  if (!ret && !logging_topic && this->is_connected()) {
    delay(0);
    ret = this->mqtt_backend_.publish(topic, payload, payload_length, qos, retain);
    delay(0);
  }

  if (!logging_topic) {
    if (ret) {
      ESP_LOGV(TAG, "Publish(topic='%s' retain=%d qos=%d)", topic, retain, qos);
      ESP_LOGVV(TAG, "Publish payload (len=%u): '%.*s'", payload_length, static_cast<int>(payload_length), payload);
    } else {
      ESP_LOGV(TAG, "Publish failed for topic='%s' (len=%u). Will retry", topic, payload_length);
      this->status_momentary_warning("publish", 1000);
    }
  }
  return ret != 0;
}

bool MQTTClientComponent::publish_json(const char *topic, const json::json_build_t &f, uint8_t qos, bool retain) {
  std::string message = json::build_json(f);
  return this->publish(topic, message.c_str(), message.length(), qos, retain);
}

void MQTTClientComponent::enable() {
  if (this->state_ != MQTT_CLIENT_DISABLED)
    return;
  ESP_LOGD(TAG, "Enabling");
  this->state_ = MQTT_CLIENT_DISCONNECTED;
  this->last_connected_ = millis();
  this->start_dnslookup_();
}

void MQTTClientComponent::disable() {
  if (this->state_ == MQTT_CLIENT_DISABLED)
    return;
  ESP_LOGD(TAG, "Disabling");
  this->state_ = MQTT_CLIENT_DISABLED;
  this->on_shutdown();
}

/** Check if the message topic matches the given subscription topic
 *
 * INFO: MQTT spec mandates that topics must not be empty and must be valid NULL-terminated UTF-8 strings.
 *
 * @param message The message topic that was received from the MQTT server. Note: this must not contain
 *                wildcard characters as mandated by the MQTT spec.
 * @param subscription The subscription topic we are matching against.
 * @param is_normal Is this a "normal" topic - Does the message topic not begin with a "$".
 * @param past_separator Are we past the first '/' topic separator.
 * @return true if the subscription topic matches the message topic, false otherwise.
 */
static bool topic_match(const char *message, const char *subscription, bool is_normal, bool past_separator) {
  // Reached end of both strings at the same time, this means we have a successful match
  if (*message == '\0' && *subscription == '\0')
    return true;

  // Either the message or the subscribe are at the end. This means they don't match.
  if (*message == '\0' || *subscription == '\0')
    return false;

  bool do_wildcards = is_normal || past_separator;

  if (*subscription == '+' && do_wildcards) {
    // single level wildcard
    // consume + from subscription
    subscription++;
    // consume everything from message until '/' found or end of string
    while (*message != '\0' && *message != '/') {
      message++;
    }
    // after this, both pointers will point to a '/' or to the end of the string

    return topic_match(message, subscription, is_normal, true);
  }

  if (*subscription == '#' && do_wildcards) {
    // multilevel wildcard - MQTT mandates that this must be at end of subscribe topic
    return true;
  }

  // this handles '/' and normal characters at the same time.
  if (*message != *subscription)
    return false;

  past_separator = past_separator || *subscription == '/';

  // consume characters
  subscription++;
  message++;

  return topic_match(message, subscription, is_normal, past_separator);
}

static bool topic_match(const char *message, const char *subscription) {
  return topic_match(message, subscription, *message != '\0' && *message != '$', false);
}

void MQTTClientComponent::on_message(const std::string &topic, const std::string &payload) {
#ifdef USE_ESP8266
  // IMPORTANT: This defer is REQUIRED to prevent stack overflow crashes on ESP8266.
  //
  // On ESP8266, this callback is invoked directly from the lwIP/AsyncTCP network stack
  // which runs in the "sys" context with a very limited stack (~4KB). By the time we
  // reach this function, the stack is already partially consumed by the network
  // processing chain: tcp_input -> AsyncClient::_recv -> AsyncMqttClient::_onMessage -> here.
  //
  // MQTT subscription callbacks can trigger arbitrary user actions (automations, HTTP
  // requests, sensor updates, etc.) which may have deep call stacks of their own.
  // For example, an HTTP request action requires: DNS lookup -> TCP connect -> TLS
  // handshake (if HTTPS) -> request formatting. This easily overflows the remaining
  // system stack space, causing a LoadStoreAlignmentCause exception or silent corruption.
  //
  // By deferring to the main loop, we ensure callbacks execute with a fresh, full-size
  // stack in the normal application context rather than the constrained network task.
  //
  // DO NOT REMOVE THIS DEFER without understanding the above. It may appear to work
  // in simple tests but will cause crashes with complex automations.
  this->defer([this, topic, payload]() {
#endif
    for (auto &subscription : this->subscriptions_) {
      if (topic_match(topic.c_str(), subscription.topic.c_str()))
        subscription.callback(topic, payload);
    }
#ifdef USE_ESP8266
  });
#endif
}

// Setters
void MQTTClientComponent::disable_log_message() { this->log_message_.topic = ""; }
bool MQTTClientComponent::is_log_message_enabled() const { return !this->log_message_.topic.empty(); }
void MQTTClientComponent::set_reboot_timeout(uint32_t reboot_timeout) { this->reboot_timeout_ = reboot_timeout; }
void MQTTClientComponent::register_mqtt_component(MQTTComponent *component) { this->children_.push_back(component); }
void MQTTClientComponent::set_log_level(int level) { this->log_level_ = level; }
void MQTTClientComponent::set_keep_alive(uint16_t keep_alive_s) { this->mqtt_backend_.set_keep_alive(keep_alive_s); }
void MQTTClientComponent::set_log_message_template(MQTTMessage &&message) { this->log_message_ = std::move(message); }
const MQTTDiscoveryInfo &MQTTClientComponent::get_discovery_info() const { return this->discovery_info_; }
void MQTTClientComponent::set_topic_prefix(const std::string &topic_prefix, const std::string &check_topic_prefix) {
  if (App.is_name_add_mac_suffix_enabled() && (topic_prefix == check_topic_prefix)) {
    char buf[ESPHOME_DEVICE_NAME_MAX_LEN + 1];
    this->topic_prefix_ = str_sanitize_to(buf, App.get_name().c_str());
  } else {
    this->topic_prefix_ = topic_prefix;
  }
}
const std::string &MQTTClientComponent::get_topic_prefix() const { return this->topic_prefix_; }
void MQTTClientComponent::set_publish_nan_as_none(bool publish_nan_as_none) {
  this->publish_nan_as_none_ = publish_nan_as_none;
}
bool MQTTClientComponent::is_publish_nan_as_none() const { return this->publish_nan_as_none_; }
void MQTTClientComponent::disable_birth_message() {
  this->birth_message_.topic = "";
  this->recalculate_availability_();
}
void MQTTClientComponent::disable_shutdown_message() {
  this->shutdown_message_.topic = "";
  this->recalculate_availability_();
}
bool MQTTClientComponent::is_discovery_enabled() const { return !this->discovery_info_.prefix.empty(); }
bool MQTTClientComponent::is_discovery_ip_enabled() const { return this->discovery_info_.discover_ip; }
const Availability &MQTTClientComponent::get_availability() { return this->availability_; }
void MQTTClientComponent::recalculate_availability_() {
  if (this->birth_message_.topic.empty() || this->birth_message_.topic != this->last_will_.topic) {
    this->availability_.topic = "";
    return;
  }
  this->availability_.topic = this->birth_message_.topic;
  this->availability_.payload_available = this->birth_message_.payload;
  this->availability_.payload_not_available = this->last_will_.payload;
}

void MQTTClientComponent::set_last_will(MQTTMessage &&message) {
  this->last_will_ = std::move(message);
  this->recalculate_availability_();
}

void MQTTClientComponent::set_birth_message(MQTTMessage &&message) {
  this->birth_message_ = std::move(message);
  this->recalculate_availability_();
}

void MQTTClientComponent::set_shutdown_message(MQTTMessage &&message) { this->shutdown_message_ = std::move(message); }

void MQTTClientComponent::set_discovery_info(std::string &&prefix, MQTTDiscoveryUniqueIdGenerator unique_id_generator,
                                             MQTTDiscoveryObjectIdGenerator object_id_generator, bool retain,
                                             bool discover_ip, bool clean) {
  this->discovery_info_.prefix = std::move(prefix);
  this->discovery_info_.discover_ip = discover_ip;
  this->discovery_info_.unique_id_generator = unique_id_generator;
  this->discovery_info_.object_id_generator = object_id_generator;
  this->discovery_info_.retain = retain;
  this->discovery_info_.clean = clean;
}

void MQTTClientComponent::disable_last_will() { this->last_will_.topic = ""; }

void MQTTClientComponent::disable_discovery() {
  this->discovery_info_ = MQTTDiscoveryInfo{
      .prefix = "",
      .retain = false,
      .discover_ip = false,
      .clean = false,
      .unique_id_generator = MQTT_LEGACY_UNIQUE_ID_GENERATOR,
      .object_id_generator = MQTT_NONE_OBJECT_ID_GENERATOR,
  };
}
void MQTTClientComponent::on_shutdown() {
  if (!this->shutdown_message_.topic.empty()) {
    yield();
    this->publish(this->shutdown_message_);
    yield();
  }
  this->mqtt_backend_.disconnect();
}

void MQTTClientComponent::set_on_connect(mqtt_on_connect_callback_t &&callback) {
  this->mqtt_backend_.set_on_connect(std::forward<mqtt_on_connect_callback_t>(callback));
}

void MQTTClientComponent::set_on_disconnect(mqtt_on_disconnect_callback_t &&callback) {
  auto callback_copy = callback;
  this->mqtt_backend_.set_on_disconnect(std::forward<mqtt_on_disconnect_callback_t>(callback));
  this->on_disconnect_.add(std::move(callback_copy));
}

#if ASYNC_TCP_SSL_ENABLED
void MQTTClientComponent::add_ssl_fingerprint(const std::array<uint8_t, SHA1_SIZE> &fingerprint) {
  this->mqtt_backend_.setSecure(true);
  this->mqtt_backend_.addServerFingerprint(fingerprint.data());
}
#endif

MQTTClientComponent *global_mqtt_client = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// MQTTMessageTrigger
MQTTMessageTrigger::MQTTMessageTrigger(std::string topic) : topic_(std::move(topic)) {}
void MQTTMessageTrigger::set_qos(uint8_t qos) { this->qos_ = qos; }
void MQTTMessageTrigger::set_payload(const std::string &payload) { this->payload_ = payload; }
void MQTTMessageTrigger::setup() {
  global_mqtt_client->subscribe(
      this->topic_,
      [this](const std::string &topic, const std::string &payload) {
        if (this->payload_.has_value() && payload != *this->payload_) {
          return;
        }

        this->trigger(payload);
      },
      this->qos_);
}
void MQTTMessageTrigger::dump_config() {
  ESP_LOGCONFIG(TAG,
                "MQTT Message Trigger:\n"
                "  Topic: '%s'\n"
                "  QoS: %u",
                this->topic_.c_str(), this->qos_);
}
float MQTTMessageTrigger::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }

}  // namespace esphome::mqtt

#endif  // USE_MQTT
