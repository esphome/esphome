#include "mqtt_client.h"
#define USE_MQTT

#ifdef USE_MQTT

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include <utility>
#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif
#include "lwip/err.h"
#include "lwip/dns.h"
#include "mqtt_component.h"

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt";

MQTTClientComponent::MQTTClientComponent() {
  global_mqtt_client = this;
  this->credentials_.client_id = App.get_name() + "-" + get_mac_address();
}

// Connection
void MQTTClientComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MQTT...");
  /*this->mqtt_client_.onMessage([this](char const *topic, char *payload, AsyncMqttClientMessageProperties properties,
                                      size_t len, size_t index, size_t total) {
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
  this->mqtt_client_.onDisconnect([this](AsyncMqttClientDisconnectReason reason) {
    this->state_ = MQTT_CLIENT_DISCONNECTED;
    this->disconnect_reason_ = reason;
  });*/
#ifdef USE_LOGGER
  if (this->is_log_message_enabled() && logger::global_logger != nullptr) {
    logger::global_logger->add_on_log_callback([this](int level, const char *tag, const char *message) {
      if (level <= this->log_level_ && this->is_connected()) {
        this->publish(this->log_message_.topic, message, strlen(message), this->log_message_.qos,
                      this->log_message_.retain);
      }
    });
  }
#endif

  this->last_connected_ = millis();
  this->start_connect_();
}
void MQTTClientComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT:");
  ESP_LOGCONFIG(TAG, "  Server Address: %s:%u", this->credentials_.address.c_str(), this->credentials_.port);
  ESP_LOGCONFIG(TAG, "  Username: " LOG_SECRET("'%s'"), this->credentials_.username.c_str());
  ESP_LOGCONFIG(TAG, "  Client ID: " LOG_SECRET("'%s'"), this->credentials_.client_id.c_str());
  if (!this->discovery_info_.prefix.empty()) {
    ESP_LOGCONFIG(TAG, "  Discovery prefix: '%s'", this->discovery_info_.prefix.c_str());
    ESP_LOGCONFIG(TAG, "  Discovery retain: %s", YESNO(this->discovery_info_.retain));
  }
  ESP_LOGCONFIG(TAG, "  Topic Prefix: '%s'", this->topic_prefix_.c_str());
  if (!this->log_message_.topic.empty()) {
    ESP_LOGCONFIG(TAG, "  Log Topic: '%s'", this->log_message_.topic.c_str());
  }
  if (!this->availability_.topic.empty()) {
    ESP_LOGCONFIG(TAG, "  Availability: '%s'", this->availability_.topic.c_str());
  }
}
bool MQTTClientComponent::can_proceed() { return this->is_connected(); }


void MQTTClientComponent::start_connect_() {
  if (!network::is_connected())
    return;

  for (auto &subscription : this->subscriptions_) {
    subscription.subscribed = false;
    subscription.resubscribe_timeout = 0;
  }

  this->status_set_warning();

  ESP_LOGI(TAG, "Connecting to MQTT...");

  conn_params_.host = credentials_.address;
  conn_params_.port = credentials_.port;
  conn_params_.client_id = credentials_.client_id;

  if (!credentials_.username.empty())
    conn_params_.username = credentials_.username;
  else
    conn_params_.username.reset();
  if (!credentials_.password.empty()) {
    std::vector<uint8_t> pwd{credentials_.password.begin(), credentials_.password.end()};
    conn_params_.password = pwd;
  } else {
    conn_params_.password.reset();
  }

  if (!last_will_.topic.empty()) {
    conn_params_.will_topic = last_will_.topic;
    std::vector<uint8_t> msg{last_will_.payload.begin(), last_will_.payload.end()};
    conn_params_.will_message = msg;
    conn_params_.will_retain = last_will_.retain;
    conn_params_.will_qos = static_cast<QOSLevel>(last_will_.qos);
  } else {
    conn_params_.will_topic = "";
    conn_params_.will_message.clear();
    conn_params_.will_retain = false;
    conn_params_.will_qos = QOSLevel::QOS0;
  }

  conn_ = make_unique<MQTTConnection>();
  ErrorCode ec = conn_->init(&conn_params_, &sess_);
  if (ec != ErrorCode::OK) {
    ESP_LOGW(TAG, "connection init failed: %d", (int) ec);
    return;
  }

  this->state_ = MQTT_CLIENT_CONNECTING;
  this->connect_begin_ = millis();
}
bool MQTTClientComponent::is_connected() {
  return this->state_ == MQTT_CLIENT_CONNECTED && this->conn_->is_connected();
}

void MQTTClientComponent::check_connected() {
  if (conn_ && !conn_->is_connected()) {
    ErrorCode ec = conn_->loop();
    if (ec != ErrorCode::OK) {
      ESP_LOGW(TAG, "check connected loop failed: %d", (int) ec);
      state_ = MQTT_CLIENT_DISCONNECTED;
    }
    return;
  }

  this->state_ = MQTT_CLIENT_CONNECTED;
  this->sent_birth_message_ = false;
  this->status_clear_warning();
  ESP_LOGI(TAG, "MQTT Connected!");
  // MQTT Client needs some time to be fully set up.
  delay(100);  // NOLINT

  this->resubscribe_subscriptions_();

  for (MQTTComponent *component : this->children_)
    component->schedule_resend_state();
}

void MQTTClientComponent::loop() {
  const uint32_t now = millis();

  switch (this->state_) {
    case MQTT_CLIENT_DISCONNECTED:
      break;
    case MQTT_CLIENT_CONNECTING:
      this->check_connected();
      break;
    case MQTT_CLIENT_CONNECTED:
      if (!this->conn_->is_connected()) {
        this->state_ = MQTT_CLIENT_DISCONNECTED;
        ESP_LOGW(TAG, "Lost MQTT Client connection!");
        this->start_connect_();
      } else {
        ErrorCode ec = conn_->loop();
        if (ec != ErrorCode::OK) {
          ESP_LOGW(TAG, "loop loop failed");
        }

        if (!this->birth_message_.topic.empty() && !this->sent_birth_message_) {
          this->sent_birth_message_ = this->publish(this->birth_message_);
        }

        this->last_connected_ = now;
        this->resubscribe_subscriptions_();
      }
      break;
  }

  if (millis() - this->last_connected_ > this->reboot_timeout_ && this->reboot_timeout_ != 0) {
    ESP_LOGE(TAG, "Can't connect to MQTT... Restarting...");
    App.reboot();
  }
}
float MQTTClientComponent::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

// Subscribe
bool MQTTClientComponent::subscribe_(const char *topic, uint8_t qos) {
  if (!this->is_connected())
    return false;

  Subscription sub{};
  sub.topic_filter = topic;
  sub.requested_qos = static_cast<QOSLevel>(qos);
  ErrorCode ec = this->conn_->subscribe({sub});

  if (ec != ErrorCode::OK) {
    ESP_LOGV(TAG, "Subscribe failed for topic='%s'. Will retry later.", topic);
    this->status_momentary_warning("subscribe", 1000);
  }

  ESP_LOGV(TAG, "subscribe(topic='%s')", topic);
  return ec == ErrorCode::OK;
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
    json::parse_json(payload, [topic, callback](JsonObject root) { callback(topic, root); });
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
  ErrorCode ec = this->conn_->unsubscribe({topic});
  if (ec != ErrorCode::OK) {
    ESP_LOGV(TAG, "Unsubscribe failed for topic='%s'.", topic.c_str());
    this->status_momentary_warning("unsubscribe", 1000);
  }
  ESP_LOGV(TAG, "unsubscribe(topic='%s')", topic.c_str());

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
  if (!this->is_connected()) {
    // critical components will re-transmit their messages
    return false;
  }
  bool logging_topic = topic == this->log_message_.topic;
  std::vector<uint8_t> msg;
  for (size_t i = 0; i < payload_length; i++) {
    msg.push_back(static_cast<uint8_t>(payload[i]));
  }
  ErrorCode ec = this->conn_->publish(topic, std::move(msg), retain, static_cast<QOSLevel>(qos));

  if (!logging_topic) {
    if (ec != ErrorCode::OK) {
      ESP_LOGV(TAG, "Publish failed for topic='%s' (len=%u). will retry later..", topic.c_str(),
               payload_length);  // NOLINT
      this->status_momentary_warning("publish", 1000);
    } else {
      ESP_LOGV(TAG, "Publish(topic='%s' payload='%s' retain=%d)", topic.c_str(), payload, retain);
    }
  }
  return ec == ErrorCode::OK;
}

bool MQTTClientComponent::publish(const MQTTMessage &message) {
  return this->publish(message.topic, message.payload, message.qos, message.retain);
}
bool MQTTClientComponent::publish_json(const std::string &topic, const json::json_build_t &f, uint8_t qos,
                                       bool retain) {
  std::string message = json::build_json(f);
  return this->publish(topic, message, qos, retain);
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
  // on ESP8266, this is called in LWiP thread; some components do not like running
  // in an ISR.
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
void MQTTClientComponent::set_keep_alive(uint16_t keep_alive_s) { conn_params_.keep_alive = keep_alive_s; }
void MQTTClientComponent::set_log_message_template(MQTTMessage &&message) { this->log_message_ = std::move(message); }
const MQTTDiscoveryInfo &MQTTClientComponent::get_discovery_info() const { return this->discovery_info_; }
void MQTTClientComponent::set_topic_prefix(std::string topic_prefix) { this->topic_prefix_ = std::move(topic_prefix); }
const std::string &MQTTClientComponent::get_topic_prefix() const { return this->topic_prefix_; }
void MQTTClientComponent::disable_birth_message() {
  this->birth_message_.topic = "";
  this->recalculate_availability_();
}
void MQTTClientComponent::disable_shutdown_message() {
  this->shutdown_message_.topic = "";
  this->recalculate_availability_();
}
bool MQTTClientComponent::is_discovery_enabled() const { return !this->discovery_info_.prefix.empty(); }
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
                                             bool retain, bool clean) {
  this->discovery_info_.prefix = std::move(prefix);
  this->discovery_info_.unique_id_generator = unique_id_generator;
  this->discovery_info_.retain = retain;
  this->discovery_info_.clean = clean;
}

void MQTTClientComponent::disable_last_will() { this->last_will_.topic = ""; }

void MQTTClientComponent::disable_discovery() {
  this->discovery_info_ = MQTTDiscoveryInfo{.prefix = "", .retain = false};
}
void MQTTClientComponent::on_shutdown() {
  if (!this->shutdown_message_.topic.empty()) {
    yield();
    this->publish(this->shutdown_message_);
    yield();
  }
  // this->mqtt_client_.disconnect(true);
}

#if ASYNC_TCP_SSL_ENABLED
void MQTTClientComponent::add_ssl_fingerprint(const std::array<uint8_t, SHA1_SIZE> &fingerprint) {
  this->mqtt_client_.setSecure(true);
  this->mqtt_client_.addServerFingerprint(fingerprint.data());
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
  ESP_LOGCONFIG(TAG, "MQTT Message Trigger:");
  ESP_LOGCONFIG(TAG, "  Topic: '%s'", this->topic_.c_str());
  ESP_LOGCONFIG(TAG, "  QoS: %u", this->qos_);
}
float MQTTMessageTrigger::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }

}  // namespace mqtt
}  // namespace esphome

#endif  // USE_MQTT
