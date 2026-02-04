#include "mqtt_backend_host.h"

#ifdef USE_MQTT
#ifdef USE_HOST

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace esphome::mqtt {

MQTTBackendHost::MQTTBackendHost() = default;

void MQTTBackendHost::set_credentials(const char *username, const char *password) {
  this->username_ = username ? username : "";
  this->password_ = password ? password : "";
}

void MQTTBackendHost::set_will(const char *topic, uint8_t qos, bool retain, const char *payload) {
  this->will_topic_ = topic ? topic : "";
  this->will_qos_ = qos;
  this->will_retain_ = retain;
  this->will_payload_ = payload ? payload : "";
}

void MQTTBackendHost::set_server(network::IPAddress ip, uint16_t port) {
  char buf[network::IP_ADDRESS_BUFFER_SIZE];
  ip.str_to(buf);
  this->host_ = buf;
  this->port_ = port;
}

void MQTTBackendHost::set_server(const char *host, uint16_t port) {
  this->host_ = host ? host : "";
  this->port_ = port;
}

bool MQTTBackendHost::open_socket_() {
  if (this->host_.empty()) {
    return false;
  }

  char port_str[6];
  snprintf(port_str, sizeof(port_str), "%u", this->port_);

  struct addrinfo hints {};
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;

  struct addrinfo *res = nullptr;
  int gai = ::getaddrinfo(this->host_.c_str(), port_str, &hints, &res);
  if (gai != 0) {
    return false;
  }

  int fd = -1;
  for (struct addrinfo *ai = res; ai != nullptr; ai = ai->ai_next) {
    fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd < 0)
      continue;

    // Blocking connect is OK here (host only), but keep it short using OS defaults.
    if (::connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
      break;
    }

    ::close(fd);
    fd = -1;
  }

  ::freeaddrinfo(res);

  if (fd < 0) {
    return false;
  }

  // mqtt-c expects a non-blocking socket.
  int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags >= 0) {
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  }

  this->socket_fd_ = fd;
  return true;
}

void MQTTBackendHost::close_socket_() {
  if (this->socket_fd_ >= 0) {
    ::close(this->socket_fd_);
    this->socket_fd_ = -1;
  }
}

void MQTTBackendHost::mark_disconnected_(MQTTClientDisconnectReason reason) {
  if (!this->connected_ && this->socket_fd_ < 0) {
    return;
  }
  this->connected_ = false;
  this->connack_seen_ = false;
  this->close_socket_();
  if (this->on_disconnect_) {
    this->on_disconnect_(reason);
  }
}

void MQTTBackendHost::connect() {
  if (this->connected_) {
    return;
  }

  this->disconnect();

  if (!this->open_socket_()) {
    this->mark_disconnected_(MQTTClientDisconnectReason::TCP_DISCONNECTED);
    return;
  }

  // Initialize mqtt-c client.
  this->connack_seen_ = false;
  mqtt_init(&this->client_, this->socket_fd_, this->tx_buf_, sizeof(this->tx_buf_), this->rx_buf_,
            sizeof(this->rx_buf_), &MQTTBackendHost::publish_callback_);
  // publish_callback_ will receive `this` as state
  this->client_.publish_response_callback_state = this;

  uint8_t connect_flags = 0;
  if (this->clean_session_) {
    connect_flags |= MQTT_CONNECT_CLEAN_SESSION;
  }
  if (!this->will_topic_.empty()) {
    connect_flags |= MQTT_CONNECT_WILL_FLAG;
    // mqtt-c defaults will QoS to 0 if not set; set explicitly.
    connect_flags &= (uint8_t) ~0x18;
    connect_flags |= (uint8_t) ((this->will_qos_ & 0x03) << 3);
    if (this->will_retain_) {
      connect_flags |= MQTT_CONNECT_WILL_RETAIN;
    }
  }

  const char *will_topic = this->will_topic_.empty() ? nullptr : this->will_topic_.c_str();
  const void *will_msg = this->will_topic_.empty() ? nullptr : (const void *) this->will_payload_.data();
  size_t will_len = this->will_topic_.empty() ? 0 : this->will_payload_.size();

  const char *username = this->username_.empty() ? nullptr : this->username_.c_str();
  const char *password = this->password_.empty() ? nullptr : this->password_.c_str();

  mqtt_connect(&this->client_, this->client_id_.empty() ? nullptr : this->client_id_.c_str(), will_topic, will_msg,
               will_len, username, password, connect_flags, this->keep_alive_);

  // Complete handshake synchronously; connack sets typical_response_time from -1.
  for (int i = 0; i < 100; i++) {  // ~100 iterations best-effort; loop() will continue later.
    auto err = mqtt_sync(&this->client_);
    if (err != MQTT_OK) {
      this->mark_disconnected_(MQTTClientDisconnectReason::TCP_DISCONNECTED);
      return;
    }
    if (this->client_.typical_response_time >= 0.0) {
      this->connack_seen_ = true;
      break;
    }
    // Small sleep to avoid spinning if broker is slow.
    ::usleep(10 * 1000);
  }

  if (!this->connack_seen_) {
    this->mark_disconnected_(MQTTClientDisconnectReason::TCP_DISCONNECTED);
    return;
  }

  this->connected_ = true;
  if (this->on_connect_) {
    this->on_connect_(false);
  }
}

void MQTTBackendHost::disconnect() {
  if (this->socket_fd_ < 0) {
    this->connected_ = false;
    this->connack_seen_ = false;
    return;
  }

  // Best effort graceful disconnect.
  mqtt_disconnect(&this->client_);
  (void) mqtt_sync(&this->client_);
  this->mark_disconnected_(MQTTClientDisconnectReason::TCP_DISCONNECTED);
}

bool MQTTBackendHost::subscribe(const char *topic, uint8_t qos) {
  if (!this->connected_ || topic == nullptr) {
    return false;
  }
  auto err = mqtt_subscribe(&this->client_, topic, qos);
  if (err != MQTT_OK) {
    return false;
  }
  (void) mqtt_sync(&this->client_);
  // mqtt-c does not expose SUBACK packet id; call callback with 0 to mirror best-effort semantics.
  if (this->on_subscribe_) {
    this->on_subscribe_(0, qos);
  }
  return true;
}

bool MQTTBackendHost::unsubscribe(const char *topic) {
  if (!this->connected_ || topic == nullptr) {
    return false;
  }
  auto err = mqtt_unsubscribe(&this->client_, topic);
  if (err != MQTT_OK) {
    return false;
  }
  (void) mqtt_sync(&this->client_);
  if (this->on_unsubscribe_) {
    this->on_unsubscribe_(0);
  }
  return true;
}

bool MQTTBackendHost::publish(const char *topic, const char *payload, size_t length, uint8_t qos, bool retain) {
  if (!this->connected_ || topic == nullptr) {
    return false;
  }

  uint8_t flags = 0;
  if (retain)
    flags |= MQTT_PUBLISH_RETAIN;
  // qos bits are 1..2 in flags:
  flags |= (uint8_t) ((qos & 0x03) << 1);

  auto err = mqtt_publish(&this->client_, topic, payload, length, flags);
  if (err != MQTT_OK) {
    return false;
  }
  (void) mqtt_sync(&this->client_);
  if (this->on_publish_) {
    this->on_publish_(0);
  }
  return true;
}

void MQTTBackendHost::loop() {
  if (!this->connected_) {
    return;
  }
  auto err = mqtt_sync(&this->client_);
  if (err != MQTT_OK) {
    this->mark_disconnected_(MQTTClientDisconnectReason::TCP_DISCONNECTED);
  }
}

void MQTTBackendHost::publish_callback_(void **state, mqtt_response_publish *publish) {
  if (state == nullptr || publish == nullptr)
    return;
  auto *self = static_cast<MQTTBackendHost *>(*state);
  if (self == nullptr)
    return;
  self->on_publish_received_(publish);
}

void MQTTBackendHost::on_publish_received_(mqtt_response_publish *publish) {
  if (!this->on_message_) {
    return;
  }
  const auto *topic_ptr = static_cast<const char *>(publish->topic_name);
  const auto *payload_ptr = static_cast<const char *>(publish->application_message);

  std::string topic(topic_ptr, topic_ptr + publish->topic_name_size);
  // mqtt-c gives us a full payload buffer; report as single chunk.
  this->on_message_(topic.c_str(), payload_ptr, publish->application_message_size, 0,
                    publish->application_message_size);
}

}  // namespace esphome::mqtt

#endif  // USE_HOST
#endif  // USE_MQTT
