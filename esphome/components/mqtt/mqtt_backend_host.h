#pragma once

#include "mqtt_backend.h"

#ifdef USE_MQTT
#ifdef USE_HOST

#include <mqtt.h>

#include <functional>
#include <string>

namespace esphome::mqtt {

class MQTTBackendHost final : public MQTTBackend {
 public:
  MQTTBackendHost();

  void set_keep_alive(uint16_t keep_alive) final { this->keep_alive_ = keep_alive; }
  void set_client_id(const char *client_id) final { this->client_id_ = client_id ? client_id : ""; }
  void set_clean_session(bool clean_session) final { this->clean_session_ = clean_session; }
  void set_credentials(const char *username, const char *password) final;
  void set_will(const char *topic, uint8_t qos, bool retain, const char *payload) final;
  void set_server(network::IPAddress ip, uint16_t port) final;
  void set_server(const char *host, uint16_t port) final;
  void set_on_connect(std::function<on_connect_callback_t> &&callback) final {
    this->on_connect_ = std::move(callback);
  }
  void set_on_disconnect(std::function<on_disconnect_callback_t> &&callback) final {
    this->on_disconnect_ = std::move(callback);
  }
  void set_on_subscribe(std::function<on_subscribe_callback_t> &&callback) final {
    this->on_subscribe_ = std::move(callback);
  }
  void set_on_unsubscribe(std::function<on_unsubscribe_callback_t> &&callback) final {
    this->on_unsubscribe_ = std::move(callback);
  }
  void set_on_message(std::function<on_message_callback_t> &&callback) final {
    this->on_message_ = std::move(callback);
  }
  void set_on_publish(std::function<on_publish_user_callback_t> &&callback) final {
    this->on_publish_ = std::move(callback);
  }

  bool connected() const final { return this->connected_; }
  void connect() final;
  void disconnect() final;
  bool subscribe(const char *topic, uint8_t qos) final;
  bool unsubscribe(const char *topic) final;
  bool publish(const char *topic, const char *payload, size_t length, uint8_t qos, bool retain) final;
  using MQTTBackend::publish;

  void loop() final;

 protected:
  static void publish_callback_(void **state, mqtt_response_publish *publish);
  void on_publish_received_(mqtt_response_publish *publish);

  bool open_socket_();
  void close_socket_();
  void mark_disconnected_(MQTTClientDisconnectReason reason);

  std::string host_;
  uint16_t port_{1883};

  std::string client_id_;
  bool clean_session_{false};
  uint16_t keep_alive_{15};

  std::string username_;
  std::string password_;

  std::string will_topic_;
  std::string will_payload_;
  uint8_t will_qos_{0};
  bool will_retain_{false};

  int socket_fd_{-1};
  bool connected_{false};

  mqtt_client client_{};
  // mqtt-c expects tx buffer to be word-aligned on some platforms. Align defensively.
  alignas(4) uint8_t tx_buf_[4096];
  uint8_t rx_buf_[4096];

  // mqtt-c sets typical_response_time to -1 until CONNACK is processed.
  bool connack_seen_{false};

  std::function<on_connect_callback_t> on_connect_{};
  std::function<on_disconnect_callback_t> on_disconnect_{};
  std::function<on_subscribe_callback_t> on_subscribe_{};
  std::function<on_unsubscribe_callback_t> on_unsubscribe_{};
  std::function<on_message_callback_t> on_message_{};
  std::function<on_publish_user_callback_t> on_publish_{};
};

}  // namespace esphome::mqtt

#endif  // USE_HOST
#endif  // USE_MQTT
