#pragma once
#include "mqtt_backend.h"

#ifdef USE_MQTT
#ifdef USE_ESP8266

#include <AsyncMqttClient.h>

namespace esphome {
namespace mqtt {

class MQTTBackendESP8266 final : public MQTTBackend {
 public:
  void set_keep_alive(uint16_t keep_alive) final { mqtt_client_.setKeepAlive(keep_alive); }
  void set_client_id(const char *client_id) final { mqtt_client_.setClientId(client_id); }
  void set_clean_session(bool clean_session) final { mqtt_client_.setCleanSession(clean_session); }
  void set_credentials(const char *username, const char *password) final {
    mqtt_client_.setCredentials(username, password);
  }
  void set_will(const char *topic, uint8_t qos, bool retain, const char *payload) final {
    mqtt_client_.setWill(topic, qos, retain, payload);
  }
  void set_server(network::IPAddress ip, uint16_t port) final { mqtt_client_.setServer(ip, port); }
  void set_server(const char *host, uint16_t port) final { mqtt_client_.setServer(host, port); }
#if ASYNC_TCP_SSL_ENABLED
  void set_secure(bool secure) { mqtt_client.setSecure(secure); }
  void add_server_fingerprint(const uint8_t *fingerprint) { mqtt_client.addServerFingerprint(fingerprint); }
#endif

  void set_on_connect(std::function<on_connect_callback_t> &&callback) final {
    this->mqtt_client_.onConnect(std::move(callback));
  }
  void set_on_disconnect(std::function<on_disconnect_callback_t> &&callback) final {
    auto async_callback = [callback](AsyncMqttClientDisconnectReason reason) {
      // int based enum so casting isn't a problem
      callback(static_cast<MQTTClientDisconnectReason>(reason));
    };
    this->mqtt_client_.onDisconnect(std::move(async_callback));
  }
  void set_on_subscribe(std::function<on_subscribe_callback_t> &&callback) final {
    this->mqtt_client_.onSubscribe(std::move(callback));
  }
  void set_on_unsubscribe(std::function<on_unsubscribe_callback_t> &&callback) final {
    this->mqtt_client_.onUnsubscribe(std::move(callback));
  }
  void set_on_message(std::function<on_message_callback_t> &&callback) final {
    auto async_callback = [callback](const char *topic, const char *payload,
                                     AsyncMqttClientMessageProperties async_properties, size_t len, size_t index,
                                     size_t total) { callback(topic, payload, len, index, total); };
    mqtt_client_.onMessage(std::move(async_callback));
  }
  void set_on_publish(std::function<on_publish_user_callback_t> &&callback) final {
    this->mqtt_client_.onPublish(std::move(callback));
  }

  bool connected() const final { return mqtt_client_.connected(); }
  void connect() final { mqtt_client_.connect(); }
  void disconnect() final { mqtt_client_.disconnect(true); }
  bool subscribe(const char *topic, uint8_t qos) final { return mqtt_client_.subscribe(topic, qos) != 0; }
  bool unsubscribe(const char *topic) final { return mqtt_client_.unsubscribe(topic) != 0; }
  bool publish(const char *topic, const char *payload, size_t length, uint8_t qos, bool retain) final {
    return mqtt_client_.publish(topic, qos, retain, payload, length, false, 0) != 0;
  }
  using MQTTBackend::publish;

 protected:
  AsyncMqttClient mqtt_client_;
};

}  // namespace mqtt
}  // namespace esphome

#endif  // defined(USE_ESP8266)
#endif
