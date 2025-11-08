#pragma once
#include "mqtt_backend.h"

#ifdef USE_MQTT
#ifdef USE_HOST

#include <string>
#include <MQTTClient.h>
#include "esphome/components/network/ip_address.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace mqtt {

class MQTTBackendHost final : public MQTTBackend {
 public:
  void set_keep_alive(uint16_t keep_alive) final { this->keep_alive_ = keep_alive; }
  void set_client_id(const char *client_id) final {
    if (client_id)
      this->client_id_ = client_id;
  }
  void set_clean_session(bool clean_session) final { this->clean_session_ = clean_session; }

  void set_credentials(const char *username, const char *password) final {
    if (username)
      this->username_ = username;
    if (password)
      this->password_ = password;
  }

  void set_will(const char *topic, uint8_t qos, bool retain, const char *payload) final {
    if (topic)
      this->lwt_topic_ = topic;
    this->lwt_qos_ = qos;
    if (payload)
      this->lwt_message_ = payload;
    this->lwt_retain_ = retain;
  }

  void set_server(network::IPAddress ip, uint16_t port) final {
    this->host_ = ip.str();
    this->port_ = port;
  }

  void set_server(const char *host, uint16_t port) final {
    this->host_ = host;
    this->port_ = port;
  }

  void set_on_connect(std::function<on_connect_callback_t> &&callback) final {
    this->on_connect_.add(std::move(callback));
  }

  void set_on_disconnect(std::function<on_disconnect_callback_t> &&callback) final {
    this->on_disconnect_.add(std::move(callback));
  }

  void set_on_subscribe(std::function<on_subscribe_callback_t> &&callback) final {
    this->on_subscribe_.add(std::move(callback));
  }

  void set_on_unsubscribe(std::function<on_unsubscribe_callback_t> &&callback) final {
    this->on_unsubscribe_.add(std::move(callback));
  }

  void set_on_message(std::function<on_message_callback_t> &&callback) final {
    this->on_message_.add(std::move(callback));
  }

  void set_on_publish(std::function<on_publish_user_callback_t> &&callback) final {
    this->on_publish_.add(std::move(callback));
  }

  bool connected() const final { return this->is_connected_; }

  void connect() final;
  void disconnect() final;
  bool subscribe(const char *topic, uint8_t qos) final;
  bool unsubscribe(const char *topic) final;
  bool publish(const char *topic, const char *payload, size_t length, uint8_t qos, bool retain) final;

  using MQTTBackend::publish;

  void loop() final;

 protected:
  static int on_message_received_(void *context, char *topic_name, int topic_len, MQTTClient_message *message);
  static void on_connection_lost_(void *context, char *cause);
  static void on_delivery_complete_(void *context, MQTTClient_deliveryToken dt);

  void initialize_();

  MQTTClient client_{nullptr};
  std::string server_uri_;
  std::string host_;
  uint16_t port_{1883};
  std::string username_;
  std::string password_;
  std::string lwt_topic_;
  std::string lwt_message_;
  uint8_t lwt_qos_{0};
  bool lwt_retain_{false};
  std::string client_id_;
  uint16_t keep_alive_{15};
  bool clean_session_{true};
  bool is_connected_{false};
  bool is_initialized_{false};

  // callbacks
  CallbackManager<on_connect_callback_t> on_connect_;
  CallbackManager<on_disconnect_callback_t> on_disconnect_;
  CallbackManager<on_subscribe_callback_t> on_subscribe_;
  CallbackManager<on_unsubscribe_callback_t> on_unsubscribe_;
  CallbackManager<on_message_callback_t> on_message_;
  CallbackManager<on_publish_user_callback_t> on_publish_;
};

}  // namespace mqtt
}  // namespace esphome

#endif  // USE_HOST
#endif  // USE_MQTT
