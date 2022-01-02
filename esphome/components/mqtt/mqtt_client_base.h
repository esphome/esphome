#pragma once

///
/// Mirror the public interface of MqttClientBase using esp-idf
///

#include <string>
#include <map>
#include "esphome/components/network/ip_address.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace mqtt {

enum class MqttEvent {
  MQTT_EVENT_ANY = -1,
  MQTT_EVENT_ERROR = 0,
  MQTT_EVENT_CONNECTED,
  MQTT_EVENT_DISCONNECTED,
  MQTT_EVENT_SUBSCRIBED,
  MQTT_EVENT_UNSUBSCRIBED,
  MQTT_EVENT_PUBLISHED,
  MQTT_EVENT_DATA,
  MQTT_EVENT_BEFORE_CONNECT,
  MQTT_EVENT_DELETED,
};

struct MqttEventT {
  char *data;              /*!< Data associated with this event */
  int data_len;            /*!< Length of the data for this event */
  int total_data_len;      /*!< Total length of the data (longer data are supplied with multiple events) */
  int current_data_offset; /*!< Actual offset for the data associated with this event */
  char *topic;             /*!< Topic associated with this event */
  int topic_len;           /*!< Length of the topic for this event associated with this event */
  int msg_id;              /*!< MQTT messaged id of message */
  int session_present;     /*!< MQTT session_present flag for connection event */
  bool retain;             /*!< Retained flag of the message associated with this event */
  int qos;                 /*!< qos of the messages associated with this event */
  bool dup;                /*!< dup flag of the message associated with this event */
};

enum class MqttClientDisconnectReason : int8_t {
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

struct MqttClientMessageProperties {
  uint8_t qos;
  // dup is only supported by the Arduino MQTT library, not ESP-IDF. It is a no-op on ESP-IDF.
  bool dup;
  bool retain;
};

class MqttClientBase {
 public:
  using on_connect_callback_t = std::function<void(bool session_present)>;
  using on_disconnect_callback_t = std::function<void(MqttClientDisconnectReason reason)>;
  using on_subscribe_callback_t = std::function<void(uint16_t packet_id, uint8_t qos)>;
  using on_unsubscribe_callback_t = std::function<void(uint16_t packet_id)>;
  using on_message_callback_t =
      std::function<void(const char *topic, const char *payload, size_t len, size_t index, size_t total)>;
  using on_publish_uer_callback_t = std::function<void(uint16_t packet_id)>;

  virtual void set_keep_alive(uint16_t keep_alive) = 0;
  virtual void set_client_id(const char *client_id) = 0;
  virtual void set_clean_session(bool clean_session) = 0;
  virtual void set_credentials(const char *username, const char *password) = 0;
  virtual void set_will(const char *topic, uint8_t qos, bool retain, const char *payload) = 0;
  virtual void set_server(network::IPAddress ip, uint16_t port) = 0;
  virtual void set_server(const char *host, uint16_t port) = 0;
  virtual void set_on_connect(const on_connect_callback_t &callback) = 0;
  virtual void set_on_disconnect(const on_disconnect_callback_t &callback) = 0;
  virtual void set_on_subscribe(const on_subscribe_callback_t &callback) = 0;
  virtual void set_on_unsubscribe(const on_unsubscribe_callback_t &callback) = 0;
  virtual void set_on_message(const on_message_callback_t &callback) = 0;
  virtual void set_on_publish(const on_publish_uer_callback_t &callback) = 0;
  virtual bool connected() const = 0;
  virtual void connect() = 0;
  virtual void disconnect() = 0;
  virtual uint16_t subscribe(const char *topic, uint8_t qos) = 0;
  virtual uint16_t unsubscribe(const char *topic) = 0;
  virtual uint16_t publish(const char *topic, const char *payload, size_t length, uint8_t qos, bool retain,
                           uint16_t message_id) = 0;

  uint16_t publish(const std::string &topic, const std::string &payload, uint8_t qos, bool retain,
                   uint16_t message_id) {
    return publish(topic.c_str(), payload.c_str(), payload.length(), qos, retain, message_id);
  }

  virtual uint16_t publish(const MQTTMessage &message) {
    return this->publish(message.topic.c_str(), message.payload.c_str(), message.payload.length(), message.qos,
                         message.retain, 0);
  }
};

}  // namespace mqtt
}  // namespace esphome
