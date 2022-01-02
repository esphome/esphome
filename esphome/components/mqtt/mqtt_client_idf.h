#pragma once

///
/// Mirror the public interface of MqttIdfClient using esp-idf
///

#ifdef USE_ESP_IDF
#include <string>
#include <map>
#include <mqtt_client.h>
#include "esphome/components/network/ip_address.h"
#include "esphome/core/helpers.h"
#include "mqtt_client_base.h"

namespace esphome {
namespace mqtt {

class MqttIdfClient : public MqttClientBase {
 public:
  static const size_t MQTT_BUFFER_SIZE = 4096;

  void set_keep_alive(uint16_t keep_alive) final { this->keep_alive_ = keep_alive; }
  void set_client_id(const char *client_id) override { this->client_id_ = client_id; }
  void set_clean_session(bool clean_session) override { this->clean_session_ = clean_session; }

  void set_credentials(const char *username, const char *password) override {
    if (username)
      this->username_ = username;
    if (password)
      this->password_ = password;
  }
  void set_will(const char *topic, uint8_t qos, bool retain, const char *payload) override {
    if (topic)
      this->lwt_topic_ = topic;
    this->lwt_qos_ = qos;
    if (payload)
      this->lwt_message_ = payload;
    this->lwt_retain_ = retain;
  }
  void set_server(network::IPAddress ip, uint16_t port) override {
    this->host_ = ip.str();
    this->port_ = port;
  }
  void set_server(const char *host, uint16_t port) override {
    this->host_ = host;
    this->port_ = port;
  }
  void set_on_connect(const on_connect_callback_t &callback) override { this->on_connect_.push_back(callback); }
  void set_on_disconnect(const on_disconnect_callback_t &callback) override {
    this->on_disconnect_.push_back(callback);
  }
  void set_on_subscribe(const on_subscribe_callback_t &callback) override { this->on_subscribe_.push_back(callback); }
  void set_on_unsubscribe(const on_unsubscribe_callback_t &callback) override {
    this->on_unsubscribe_.push_back(callback);
  }
  void set_on_message(const on_message_callback_t &callback) override { this->on_message_.push_back(callback); }
  void set_on_publish(const on_publish_uer_callback_t &callback) override { this->on_publish_.push_back(callback); }
  bool connected() const override { return this->is_connected_; }

  void connect() override {
    if (!is_initalized_)
      if (initialize_())
        esp_mqtt_client_start(handler_.get());
  }
  void disconnect() override {
    if (is_initalized_)
      esp_mqtt_client_disconnect(handler_.get());
  }

  uint16_t subscribe(const char *topic, uint8_t qos) override {
    return map_status_(esp_mqtt_client_subscribe(handler_.get(), topic, qos));
  }
  uint16_t unsubscribe(const char *topic) override {
    return map_status_(esp_mqtt_client_unsubscribe(handler_.get(), topic));
  }
  /**
   *
   * @param properties dup is ignored for ESP-IDF
   */
  uint16_t publish(const char *topic, const char *payload, size_t length, uint8_t qos, bool retain,
                   uint16_t message_id) final {
#if defined IDF_MQTT_USE_ENQUEUE
    // use the non-blocking version
    // it can delay sending a couple of seconds but won't block
    auto status = map_status_(esp_mqtt_client_enqueue(handler_.get(), topic, payload, length, qos, retain, true));
#else
    // might block for several seconds, either due to network timeout (10s)
    // or if publishing payloads longer than internal buffer (due to message fragmentation)
    auto status = map_status_(esp_mqtt_client_publish(handler_.get(), topic, payload, length, qos, retain));
#endif
    return status;
  }
  uint16_t publish(const MQTTMessage &message) override {
#if defined IDF_MQTT_USE_ENQUEUE
    // use the non-blocking version
    // it can delay sending a couple of seconds but won't block
    auto status = map_status_(esp_mqtt_client_enqueue(handler_.get(), message.topic.c_str(), message.payload.c_str(),
                                                      message.payload.length(), message.qos, message.retain, true));
#else
    // might block for several seconds, either due to network timeout (10s)
    // or if publishing payloads longer than internal buffer (due to message fragmentation)
    auto status = map_status_(esp_mqtt_client_publish(handler_.get(), message.topic.c_str(), message.payload.c_str(),
                                                      message.payload.length(), message.qos, message.retain));

#endif
    return status;
  }

  /// End - Required for compability
  void set_ca_certificate(const std::string &cert) { ca_certificate_ = cert; }
  void set_skip_cert_cn_check(bool skip_check) { skip_cert_cn_check_ = skip_check; }

 protected:
  // MqttClient uses 0 for an error and 1 for QoS 0 message ids
  // idf ues -1 for error and 0 for QoS 0
  inline uint16_t map_status_(int status) {
    if (status == -1)
      status = 0;
    if (status == 0)
      status = 1;
    return status;
  }
  bool initialize_();
  void mqtt_event_handler_(esp_event_base_t base, int32_t event_id, void *event_data);
  static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

  struct MqttClientDeleter {
    void operator()(esp_mqtt_client *client_handler) { esp_mqtt_client_destroy(client_handler); }
  };
  using ClientHandler_ = std::unique_ptr<esp_mqtt_client, MqttClientDeleter>;
  ClientHandler_ handler_;

  bool is_connected_{false};
  bool is_initalized_{false};

  esp_mqtt_client_config_t mqtt_cfg_{};

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
  optional<std::string> ca_certificate_;
  bool skip_cert_cn_check_{false};

  // callbacks
  std::vector<on_connect_callback_t> on_connect_;
  std::vector<on_disconnect_callback_t> on_disconnect_;
  std::vector<on_subscribe_callback_t> on_subscribe_;
  std::vector<on_unsubscribe_callback_t> on_unsubscribe_;
  std::vector<on_message_callback_t> on_message_;
  std::vector<on_publish_uer_callback_t> on_publish_;
};

}  // namespace mqtt
}  // namespace esphome
#endif
