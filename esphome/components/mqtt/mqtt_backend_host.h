#pragma once

#include "mqtt_backend.h"

#ifdef USE_MQTT
#ifdef USE_HOST

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "esphome/components/socket/socket.h"
#include "esphome/core/defines.h"

namespace esphome::mqtt {

class MQTTBackendHost final : public MQTTBackend {
 public:
  void set_keep_alive(uint16_t keep_alive) final { this->keep_alive_s_ = keep_alive; }
  void set_client_id(const char *client_id) final { this->client_id_ = client_id != nullptr ? client_id : ""; }
  void set_clean_session(bool clean_session) final { this->clean_session_ = clean_session; }
  void set_credentials(const char *username, const char *password) final {
    this->username_ = username != nullptr ? username : "";
    this->password_ = password != nullptr ? password : "";
  }
  void set_will(const char *topic, uint8_t qos, bool retain, const char *payload) final {
    this->will_topic_ = topic != nullptr ? topic : "";
    this->will_payload_ = payload != nullptr ? payload : "";
    this->will_qos_ = qos;
    this->will_retain_ = retain;
  }
  void set_server(network::IPAddress ip, uint16_t port) final;
  void set_server(const char *host, uint16_t port) final;
#
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
#
  bool connected() const final;
  void connect() final;
  void disconnect() final;
  bool subscribe(const char *topic, uint8_t qos) final;
  bool unsubscribe(const char *topic) final;
  bool publish(const char *topic, const char *payload, size_t length, uint8_t qos, bool retain) final;
  void loop() final;
#
  using MQTTBackend::publish;
#
 protected:
  void close_socket_();
  void handle_disconnect_(MQTTClientDisconnectReason reason);
#
  bool send_connect_();
  bool send_pingreq_();
  bool send_disconnect_();
#
  bool send_subscribe_(uint16_t packet_id, const char *topic, uint8_t qos);
  bool send_unsubscribe_(uint16_t packet_id, const char *topic);
#
  bool send_publish_(const char *topic, const char *payload, size_t length, bool retain);
#
  bool send_packet_(const uint8_t *data, size_t len);
  void process_rx_();
#
  static void append_u16_(std::vector<uint8_t> &out, uint16_t value);
  static void append_mqtt_string_(std::vector<uint8_t> &out, const std::string &value);
  static void append_remaining_length_(std::vector<uint8_t> &out, size_t value);
  static bool decode_remaining_length_(const uint8_t *data, size_t len, size_t *value, size_t *used);
#
  bool resolve_host_ipv4_(struct sockaddr_in *out) const;
#
  std::unique_ptr<socket::Socket> socket_;
  bool connected_{false};
#
  uint16_t keep_alive_s_{15};
  uint32_t last_io_ms_{0};
#
  std::string server_host_{};
  network::IPAddress server_ip_{};
  bool server_ip_set_{false};
  uint16_t server_port_{1883};
#
  std::string client_id_{};
  bool clean_session_{false};
#
  std::string username_{};
  std::string password_{};
#
  std::string will_topic_{};
  std::string will_payload_{};
  uint8_t will_qos_{0};
  bool will_retain_{false};
#
  uint16_t next_packet_id_{1};
#
  std::vector<uint8_t> rx_buffer_{};
#
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
