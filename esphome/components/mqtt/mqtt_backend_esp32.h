#pragma once

#include "mqtt_backend.h"
#ifdef USE_MQTT
#ifdef USE_ESP32

#include <string>
#include <queue>
#include <mqtt_client.h>
#include "esphome/components/network/ip_address.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace mqtt {

struct Event {
  esp_mqtt_event_id_t event_id;
  std::vector<char> data;
  int total_data_len;
  int current_data_offset;
  std::string topic;
  int msg_id;
  bool retain;
  int qos;
  bool dup;
  bool session_present;
  esp_mqtt_error_codes_t error_handle;

  // Construct from esp_mqtt_event_t
  // Any pointer values that are unsafe to keep are converted to safe copies
  Event(const esp_mqtt_event_t &event)
      : event_id(event.event_id),
        data(event.data, event.data + event.data_len),
        total_data_len(event.total_data_len),
        current_data_offset(event.current_data_offset),
        topic(event.topic, event.topic_len),
        msg_id(event.msg_id),
        retain(event.retain),
        qos(event.qos),
        dup(event.dup),
        session_present(event.session_present),
        error_handle(*event.error_handle) {}
};

class MQTTBackendESP32 final : public MQTTBackend {
 public:
  static const size_t MQTT_BUFFER_SIZE = 4096;

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

  void connect() final {
    if (!is_initalized_) {
      if (initialize_()) {
        esp_mqtt_client_start(handler_.get());
      }
    }
  }
  void disconnect() final {
    if (is_initalized_)
      esp_mqtt_client_disconnect(handler_.get());
  }

  bool subscribe(const char *topic, uint8_t qos) final {
    return esp_mqtt_client_subscribe(handler_.get(), topic, qos) != -1;
  }
  bool unsubscribe(const char *topic) final { return esp_mqtt_client_unsubscribe(handler_.get(), topic) != -1; }

  bool publish(const char *topic, const char *payload, size_t length, uint8_t qos, bool retain) final {
#if defined(USE_MQTT_IDF_ENQUEUE)
    // use the non-blocking version
    // it can delay sending a couple of seconds but won't block
    return esp_mqtt_client_enqueue(handler_.get(), topic, payload, length, qos, retain, true) != -1;
#else
    // might block for several seconds, either due to network timeout (10s)
    // or if publishing payloads longer than internal buffer (due to message fragmentation)
    return esp_mqtt_client_publish(handler_.get(), topic, payload, length, qos, retain) != -1;
#endif
  }
  using MQTTBackend::publish;

  void loop() final;

  void set_ca_certificate(const std::string &cert) { ca_certificate_ = cert; }
  void set_cl_certificate(const std::string &cert) { cl_certificate_ = cert; }
  void set_cl_key(const std::string &key) { cl_key_ = key; }
  void set_skip_cert_cn_check(bool skip_check) { skip_cert_cn_check_ = skip_check; }

 protected:
  bool initialize_();
  void mqtt_event_handler_(const Event &event);
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
  optional<std::string> cl_certificate_;
  optional<std::string> cl_key_;
  bool skip_cert_cn_check_{false};

  // callbacks
  CallbackManager<on_connect_callback_t> on_connect_;
  CallbackManager<on_disconnect_callback_t> on_disconnect_;
  CallbackManager<on_subscribe_callback_t> on_subscribe_;
  CallbackManager<on_unsubscribe_callback_t> on_unsubscribe_;
  CallbackManager<on_message_callback_t> on_message_;
  CallbackManager<on_publish_user_callback_t> on_publish_;
  std::queue<Event> mqtt_events_;
};

}  // namespace mqtt
}  // namespace esphome

#endif
#endif
