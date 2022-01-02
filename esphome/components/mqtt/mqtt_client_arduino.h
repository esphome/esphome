#pragma once
#if defined(USE_ARDUINO)
#include "mqtt_client_base.h"
#include <AsyncMqttClient.h>
#include "lwip/ip_addr.h"

namespace esphome {
namespace mqtt {

class MQTTArduinoClient : public MqttClientBase {
 public:
  virtual ~MQTTArduinoClient() = default;
  void set_keep_alive(uint16_t keep_alive) final { mqtt_client_.setKeepAlive(keep_alive); }
  void set_client_id(const char *client_id) final { mqtt_client_.setClientId(client_id); }
  void set_clean_session(bool clean_session) final { mqtt_client_.setCleanSession(clean_session); }
  void set_max_topic_length(uint16_t max_topic_length) { mqtt_client_.setMaxTopicLength(max_topic_length); };
  void set_credentials(const char *username, const char *password) final {
    mqtt_client_.setCredentials(username, password);
  }
  void set_will(const char *topic, uint8_t qos, bool retain, const char *payload) final {
    mqtt_client_.setWill(topic, qos, retain, payload);
  }
  void set_server(network::IPAddress ip, uint16_t port) final {
    mqtt_client_.setServer(IPAddress(static_cast<uint32_t>(ip)), port);
  }
  void set_server(const char *host, uint16_t port) final { mqtt_client_.setServer(host, port); }
#if ASYNC_TCP_SSL_ENABLED
  void set_secure(bool secure) { mqtt_client.setSecure(secure); }
  void add_server_fingerprint(const uint8_t *fingerprint) { mqtt_client.addServerFingerprint(fingerprint); }
#endif

  void set_on_connect(const on_connect_callback_t &callback) final { this->mqtt_client_.onConnect(callback); }
  void set_on_disconnect(const on_disconnect_callback_t &callback) final {
    std::function<void(AsyncMqttClientDisconnectReason)> async_callback =
        [callback](AsyncMqttClientDisconnectReason reason) {
          // int based enum so casting isn't a problem
          callback(static_cast<MqttClientDisconnectReason>(reason));
        };
    this->mqtt_client_.onDisconnect(async_callback);
  }
  void set_on_subscribe(const on_subscribe_callback_t &callback) final { this->mqtt_client_.onSubscribe(callback); }
  void set_on_unsubscribe(const on_unsubscribe_callback_t &callback) final {
    this->mqtt_client_.onUnsubscribe(callback);
  }
  void set_on_message(const on_message_callback_t &callback) final {
    std::function<void(const char *topic, const char *payload, AsyncMqttClientMessageProperties async_properties,
                       size_t len, size_t index, size_t total)>
        async_callback = [callback](const char *topic, const char *payload,
                                    AsyncMqttClientMessageProperties async_properties, size_t len, size_t index,
                                    size_t total) { callback(topic, payload, len, index, total); };
    mqtt_client_.onMessage(async_callback);
  }
  void set_on_publish(const on_publish_uer_callback_t &callback) final { this->mqtt_client_.onPublish(callback); }

  bool connected() const final { return mqtt_client_.connected(); }
  void connect() final { mqtt_client_.connect(); }
  void disconnect() final { mqtt_client_.disconnect(true); }
  uint16_t subscribe(const char *topic, uint8_t qos) final { return mqtt_client_.subscribe(topic, qos); }
  uint16_t unsubscribe(const char *topic) final { return mqtt_client_.unsubscribe(topic); }
  uint16_t publish(const char *topic, const char *payload, size_t length, uint8_t qos, bool retain,
                   uint16_t message_id) final {
    return mqtt_client_.publish(topic, qos, retain, payload, length, false, message_id);
  }
  uint16_t publish(const MQTTMessage &msg) final {
    return mqtt_client_.publish(msg.topic.c_str(), msg.qos, msg.retain, msg.payload.c_str(), msg.payload.length(),
                                msg.retain, 0);
  }

 protected:
  AsyncMqttClient mqtt_client_;
};

}  // namespace mqtt
}  // namespace esphome

#endif  // defined(USE_ARDUINO)
