#include "mqtt_backend_host.h"

#ifdef USE_MQTT
#ifdef USE_HOST

#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.host";

void MQTTBackendHost::initialize_() {
  if (this->is_initialized_)
    return;

  // Build server URI
  this->server_uri_ = "tcp://" + this->host_ + ":" + std::to_string(this->port_);

  ESP_LOGCONFIG(TAG, "Initializing MQTT client");
  ESP_LOGCONFIG(TAG, "  Server URI: %s", this->server_uri_.c_str());
  ESP_LOGCONFIG(TAG, "  Client ID: %s", this->client_id_.c_str());

  // Create MQTT client
  int rc = MQTTClient_create(&this->client_, this->server_uri_.c_str(), this->client_id_.c_str(),
                             MQTTCLIENT_PERSISTENCE_NONE, nullptr);

  if (rc != MQTTCLIENT_SUCCESS) {
    ESP_LOGE(TAG, "Failed to create MQTT client: %d", rc);
    return;
  }

  // Set callbacks
  rc = MQTTClient_setCallbacks(this->client_, this, on_connection_lost_, on_message_received_, on_delivery_complete_);

  if (rc != MQTTCLIENT_SUCCESS) {
    ESP_LOGE(TAG, "Failed to set MQTT callbacks: %d", rc);
    MQTTClient_destroy(&this->client_);
    this->client_ = nullptr;
    return;
  }

  this->is_initialized_ = true;
}

void MQTTBackendHost::connect() {
  this->initialize_();

  if (!this->is_initialized_) {
    ESP_LOGE(TAG, "Cannot connect: MQTT client not initialized");
    return;
  }

  ESP_LOGD(TAG, "Connecting to MQTT broker...");
  ESP_LOGD(TAG, "  Server: %s:%u", this->host_.c_str(), this->port_);
  ESP_LOGD(TAG, "  Client ID: %s", this->client_id_.c_str());

  MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
  conn_opts.keepAliveInterval = this->keep_alive_;
  conn_opts.cleansession = this->clean_session_ ? 1 : 0;

  if (!this->username_.empty()) {
    conn_opts.username = this->username_.c_str();
    if (!this->password_.empty()) {
      conn_opts.password = this->password_.c_str();
    }
  }

  // Set last will and testament
  MQTTClient_willOptions will_opts = MQTTClient_willOptions_initializer;
  if (!this->lwt_topic_.empty()) {
    will_opts.topicName = this->lwt_topic_.c_str();
    will_opts.message = this->lwt_message_.c_str();
    will_opts.qos = this->lwt_qos_;
    will_opts.retained = this->lwt_retain_ ? 1 : 0;
    conn_opts.will = &will_opts;
  }

  int rc = MQTTClient_connect(this->client_, &conn_opts);

  if (rc != MQTTCLIENT_SUCCESS) {
    ESP_LOGE(TAG, "Failed to connect to MQTT broker: %d", rc);
    this->is_connected_ = false;

    // Map error code to disconnect reason
    MQTTClientDisconnectReason reason = MQTTClientDisconnectReason::TCP_DISCONNECTED;
    if (rc == 1)
      reason = MQTTClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION;
    else if (rc == 2)
      reason = MQTTClientDisconnectReason::MQTT_IDENTIFIER_REJECTED;
    else if (rc == 3)
      reason = MQTTClientDisconnectReason::MQTT_SERVER_UNAVAILABLE;
    else if (rc == 4)
      reason = MQTTClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS;
    else if (rc == 5)
      reason = MQTTClientDisconnectReason::MQTT_NOT_AUTHORIZED;

    this->on_disconnect_.call(reason);
    return;
  }

  ESP_LOGI(TAG, "Connected to MQTT broker");
  this->is_connected_ = true;
  this->on_connect_.call(false);  // session_present - we don't have access to this with Paho C
}

void MQTTBackendHost::disconnect() {
  if (!this->is_initialized_ || !this->client_) {
    ESP_LOGD(TAG, "Cannot disconnect: client not initialized");
    return;
  }

  ESP_LOGD(TAG, "Disconnecting from MQTT broker");

  int rc = MQTTClient_disconnect(this->client_, 10000);  // 10 second timeout

  if (rc != MQTTCLIENT_SUCCESS) {
    ESP_LOGW(TAG, "Failed to disconnect cleanly: %d", rc);
  }

  this->is_connected_ = false;
  this->on_disconnect_.call(MQTTClientDisconnectReason::TCP_DISCONNECTED);
}

bool MQTTBackendHost::subscribe(const char *topic, uint8_t qos) {
  if (!this->is_connected_ || !this->client_) {
    ESP_LOGW(TAG, "Cannot subscribe: not connected");
    return false;
  }

  ESP_LOGD(TAG, "Subscribing to topic: %s (QoS %u)", topic, qos);

  int rc = MQTTClient_subscribe(this->client_, topic, qos);

  if (rc != MQTTCLIENT_SUCCESS) {
    ESP_LOGE(TAG, "Failed to subscribe to topic '%s': %d", topic, rc);
    return false;
  }

  ESP_LOGD(TAG, "Successfully subscribed to topic: %s", topic);
  this->on_subscribe_.call(0, qos);  // Paho C doesn't provide packet IDs easily
  return true;
}

bool MQTTBackendHost::unsubscribe(const char *topic) {
  if (!this->is_connected_ || !this->client_) {
    ESP_LOGW(TAG, "Cannot unsubscribe: not connected");
    return false;
  }

  ESP_LOGD(TAG, "Unsubscribing from topic: %s", topic);

  int rc = MQTTClient_unsubscribe(this->client_, topic);

  if (rc != MQTTCLIENT_SUCCESS) {
    ESP_LOGE(TAG, "Failed to unsubscribe from topic '%s': %d", topic, rc);
    return false;
  }

  ESP_LOGD(TAG, "Successfully unsubscribed from topic: %s", topic);
  this->on_unsubscribe_.call(0);  // Paho C doesn't provide packet IDs easily
  return true;
}

bool MQTTBackendHost::publish(const char *topic, const char *payload, size_t length, uint8_t qos, bool retain) {
  if (!this->is_connected_ || !this->client_) {
    ESP_LOGW(TAG, "Cannot publish: not connected");
    return false;
  }

  ESP_LOGD(TAG, "Publishing to topic: %s (QoS %u, retain %d, %zu bytes)", topic, qos, retain, length);

  MQTTClient_message pubmsg = MQTTClient_message_initializer;
  pubmsg.payload = const_cast<char *>(payload);
  pubmsg.payloadlen = static_cast<int>(length);
  pubmsg.qos = qos;
  pubmsg.retained = retain ? 1 : 0;

  MQTTClient_deliveryToken token;
  int rc = MQTTClient_publishMessage(this->client_, topic, &pubmsg, &token);

  if (rc != MQTTCLIENT_SUCCESS) {
    ESP_LOGE(TAG, "Failed to publish to topic '%s': %d", topic, rc);
    return false;
  }

  // For QoS > 0, wait for delivery confirmation
  if (qos > 0) {
    rc = MQTTClient_waitForCompletion(this->client_, token, 1000);  // 1 second timeout
    if (rc != MQTTCLIENT_SUCCESS) {
      ESP_LOGW(TAG, "Failed to wait for publish completion: %d", rc);
    }
  }

  ESP_LOGV(TAG, "Successfully published to topic: %s", topic);
  this->on_publish_.call(token);
  return true;
}

void MQTTBackendHost::loop() {
  // Paho C client handles events in callbacks, but we should check connection status
  if (this->is_initialized_ && this->client_) {
    int is_connected = MQTTClient_isConnected(this->client_);
    if (is_connected && !this->is_connected_) {
      // Reconnected
      this->is_connected_ = true;
      this->on_connect_.call(false);
    } else if (!is_connected && this->is_connected_) {
      // Disconnected
      this->is_connected_ = false;
      this->on_disconnect_.call(MQTTClientDisconnectReason::TCP_DISCONNECTED);
    }
  }
}

// Static callback functions
void MQTTBackendHost::on_connection_lost_(void *context, char *cause) {
  auto *backend = static_cast<MQTTBackendHost *>(context);
  ESP_LOGW(TAG, "MQTT connection lost: %s", cause ? cause : "unknown");

  backend->is_connected_ = false;
  backend->on_disconnect_.call(MQTTClientDisconnectReason::TCP_DISCONNECTED);
}

int MQTTBackendHost::on_message_received_(void *context, char *topic_name, int topic_len, MQTTClient_message *message) {
  auto *backend = static_cast<MQTTBackendHost *>(context);

  // topic_len might be 0 if topic is null-terminated
  size_t actual_topic_len = topic_len > 0 ? topic_len : strlen(topic_name);

  ESP_LOGV(TAG, "MQTT message received: topic='%.*s', payload_len=%d", (int) actual_topic_len, topic_name,
           message->payloadlen);

  // Call the message callback
  backend->on_message_.call(topic_name, static_cast<const char *>(message->payload), message->payloadlen, 0,
                            message->payloadlen);

  MQTTClient_freeMessage(&message);
  MQTTClient_free(topic_name);
  return 1;  // Indicates message was handled
}

void MQTTBackendHost::on_delivery_complete_(void *context, MQTTClient_deliveryToken dt) {
  auto *backend = static_cast<MQTTBackendHost *>(context);
  ESP_LOGV(TAG, "MQTT delivery complete: token=%d", dt);
  backend->on_publish_.call(dt);
}

}  // namespace mqtt
}  // namespace esphome

#endif  // USE_HOST
#endif  // USE_MQTT
