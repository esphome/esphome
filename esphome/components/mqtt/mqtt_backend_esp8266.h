#pragma once
#include "mqtt_backend.h"

#ifdef USE_MQTT
#ifdef USE_ESP8266

#include "mqtt_backend.h"

#include "esphome/core/log.h"

#include <BearSSLHelpers.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

namespace esphome {
namespace mqtt {

class MQTTBackendESP8266 final : public MQTTBackend {
 public:
  void set_keep_alive(uint16_t keep_alive) final { this->keep_alive_ = keep_alive; }
  void set_client_id(const char *client_id) final { this->client_id_ = client_id; }
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
    this->mqtt_client_.setServer(ip, port);
  }
  void set_server(const char *host, uint16_t port) final {
    this->host_ = host;
    this->port_ = port;
    this->mqtt_client_.setServer(this->host_.c_str(), port);
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
  void connect() final {
    if (!this->is_initalized_) {
      this->initialize_();
    }
    this->mqtt_client_.connect(this->client_id_.c_str(), this->username_.c_str(), this->password_.c_str(),
                               this->lwt_topic_.c_str(), this->lwt_qos_, this->lwt_retain_, this->lwt_message_.c_str(),
                               this->clean_session_);
  }
  void disconnect() final {
    if (this->is_initalized_)
      this->mqtt_client_.disconnect();
  }
  bool subscribe(const char *topic, uint8_t qos) final { return this->mqtt_client_.subscribe(topic, qos); }
  bool unsubscribe(const char *topic) final { return this->mqtt_client_.unsubscribe(topic); }
  bool publish(const char *topic, const char *payload, size_t length, uint8_t qos, bool retain) final {
    /* qos parameter is ignored, as PubSubClient can only publish QoS 0 messages */
    return this->mqtt_client_.publish(topic, reinterpret_cast<const uint8_t *>(payload), length, retain);
  }
  using MQTTBackend::publish;

  void loop() final;

  void set_ca_certificate(const std::string &cert) { this->ca_certificate_str_ = cert; }
  void set_skip_cert_cn_check(bool skip_check) { this->skip_cert_cn_check_ = skip_check; }

 protected:
  void initialize_();
  static void on_mqtt_message_wrapper_(const char *topic, unsigned char *payload, unsigned int length);
  void on_mqtt_message_(const char *topic, const char *payload, unsigned int length);

#ifdef USE_MQTT_SECURE_CLIENT
  WiFiClientSecure wifi_client_;
#else
  WiFiClient wifi_client_;
#endif
  PubSubClient mqtt_client_{wifi_client_};

  bool is_connected_{false};
  bool is_initalized_{false};

  std::string host_;
  uint16_t port_;
  std::string username_;
  std::string password_;
  std::string lwt_topic_;
  std::string lwt_message_;
  uint8_t lwt_qos_;
  bool lwt_retain_;
  std::string client_id_;
  uint16_t keep_alive_;
  bool clean_session_;
  optional<std::string> ca_certificate_str_;
  BearSSL::X509List ca_certificate_;
  bool skip_cert_cn_check_{false};

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

#endif  // defined(USE_ESP8266)
#endif
