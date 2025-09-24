#pragma once
#include "esphome/core/defines.h"
#ifdef USE_MQTT
#include <string>
#include <map>
#include "esphome/components/network/ip_address.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace mqtt {

enum class MQTTClientDisconnectReason : int8_t {
  TCP_DISCONNECTED = 0,
  MQTT_UNACCEPTABLE_PROTOCOL_VERSION = 1,
  MQTT_IDENTIFIER_REJECTED = 2,
  MQTT_SERVER_UNAVAILABLE = 3,
  MQTT_MALFORMED_CREDENTIALS = 4,
  MQTT_NOT_AUTHORIZED = 5,
  ESP8266_NOT_ENOUGH_SPACE = 6,
  TLS_BAD_FINGERPRINT = 7
};

/// internal struct for MQTT messages.
struct MQTTMessage {
  std::string topic;
  std::string payload;
  uint8_t qos;  ///< QoS. Only for last will testaments.
  bool retain;
};

class MQTTBackend {
 public:
  using on_connect_callback_t = void(bool session_present);
  using on_disconnect_callback_t = void(MQTTClientDisconnectReason reason);
  using on_subscribe_callback_t = void(uint16_t packet_id, uint8_t qos);
  using on_unsubscribe_callback_t = void(uint16_t packet_id);
  using on_message_callback_t = void(const char *topic, const char *payload, size_t len, size_t index, size_t total);
  using on_publish_user_callback_t = void(uint16_t packet_id);

  virtual void set_keep_alive(uint16_t keep_alive) = 0;
  virtual void set_client_id(const char *client_id) = 0;
  virtual void set_clean_session(bool clean_session) = 0;
  virtual void set_credentials(const char *username, const char *password) = 0;
  virtual void set_will(const char *topic, uint8_t qos, bool retain, const char *payload) = 0;
  virtual void set_server(network::IPAddress ip, uint16_t port) = 0;
  virtual void set_server(const char *host, uint16_t port) = 0;
  virtual void set_on_connect(std::function<on_connect_callback_t> &&callback) = 0;
  virtual void set_on_disconnect(std::function<on_disconnect_callback_t> &&callback) = 0;
  virtual void set_on_subscribe(std::function<on_subscribe_callback_t> &&callback) = 0;
  virtual void set_on_unsubscribe(std::function<on_unsubscribe_callback_t> &&callback) = 0;
  virtual void set_on_message(std::function<on_message_callback_t> &&callback) = 0;
  virtual void set_on_publish(std::function<on_publish_user_callback_t> &&callback) = 0;
  virtual bool connected() const = 0;
  virtual void connect() = 0;
  virtual void disconnect() = 0;
  virtual bool subscribe(const char *topic, uint8_t qos) = 0;
  virtual bool unsubscribe(const char *topic) = 0;
  virtual bool publish(const char *topic, const char *payload, size_t length, uint8_t qos, bool retain) = 0;

  virtual bool publish(const MQTTMessage &message) {
    return publish(message.topic.c_str(), message.payload.c_str(), message.payload.length(), message.qos,
                   message.retain);
  }

  // called from MQTTClient::loop()
  virtual void loop() {}
};

}  // namespace mqtt
}  // namespace esphome
#endif
