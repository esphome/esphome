#include "mqtt_backend_host.h"

#ifdef USE_MQTT
#ifdef USE_HOST

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>

#include "esphome/core/hal.h"

namespace esphome::mqtt {

static const uint32_t IO_TIMEOUT_MS = 5000;

void MQTTBackendHost::set_server(network::IPAddress ip, uint16_t port) {
  this->server_ip_ = ip;
  this->server_ip_set_ = true;
  this->server_host_.clear();
  this->server_port_ = port;
}

void MQTTBackendHost::set_server(const char *host, uint16_t port) {
  this->server_host_ = host != nullptr ? host : "";
  this->server_ip_set_ = false;
  this->server_port_ = port;
}

bool MQTTBackendHost::connected() const { return this->connected_ && this->socket_ != nullptr; }

void MQTTBackendHost::close_socket_() { this->socket_.reset(); }

void MQTTBackendHost::handle_disconnect_(MQTTClientDisconnectReason reason) {
  if (this->socket_ == nullptr && !this->connected_)
    return;
  this->connected_ = false;
  this->close_socket_();
  if (this->on_disconnect_) {
    this->on_disconnect_(reason);
  }
}

void MQTTBackendHost::connect() {
  this->disconnect();

  struct sockaddr_in addr {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(this->server_port_);

  if (!this->server_host_.empty()) {
    if (!this->resolve_host_ipv4_(&addr)) {
      this->handle_disconnect_(MQTTClientDisconnectReason::DNS_RESOLVE_ERROR);
      return;
    }
  } else if (this->server_ip_set_) {
    char ip_buf[network::IP_ADDRESS_BUFFER_SIZE];
    this->server_ip_.str_to(ip_buf);
    if (inet_aton(ip_buf, &addr.sin_addr) == 0) {
      this->handle_disconnect_(MQTTClientDisconnectReason::DNS_RESOLVE_ERROR);
      return;
    }
  } else {
    this->handle_disconnect_(MQTTClientDisconnectReason::DNS_RESOLVE_ERROR);
    return;
  }

  auto sock = socket::socket(AF_INET, SOCK_STREAM, 0);
  if (sock == nullptr) {
    this->handle_disconnect_(MQTTClientDisconnectReason::TCP_DISCONNECTED);
    return;
  }

  // Apply conservative timeouts so host mode can't hang indefinitely.
  struct timeval tv {};
  tv.tv_sec = IO_TIMEOUT_MS / 1000;
  tv.tv_usec = (IO_TIMEOUT_MS % 1000) * 1000;
  (void) sock->setsockopt(SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  (void) sock->setsockopt(SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  if (sock->connect(reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) != 0) {
    this->handle_disconnect_(MQTTClientDisconnectReason::TCP_DISCONNECTED);
    return;
  }

  // Switch to non-blocking after connect to avoid stalling the main loop.
  (void) sock->setblocking(false);

  this->socket_ = std::move(sock);
  this->connected_ = false;
  this->rx_buffer_.clear();
  this->last_io_ms_ = millis();

  if (!this->send_connect_()) {
    this->handle_disconnect_(MQTTClientDisconnectReason::TCP_DISCONNECTED);
  }
}

void MQTTBackendHost::disconnect() {
  if (this->socket_ != nullptr) {
    (void) this->send_disconnect_();
  }
  this->connected_ = false;
  this->close_socket_();
}

bool MQTTBackendHost::subscribe(const char *topic, uint8_t qos) {
  if (this->socket_ == nullptr)
    return false;
  uint16_t packet_id = this->next_packet_id_++;
  bool ok = this->send_subscribe_(packet_id, topic, qos);
  this->last_io_ms_ = millis();
  return ok;
}

bool MQTTBackendHost::unsubscribe(const char *topic) {
  if (this->socket_ == nullptr)
    return false;
  uint16_t packet_id = this->next_packet_id_++;
  bool ok = this->send_unsubscribe_(packet_id, topic);
  this->last_io_ms_ = millis();
  return ok;
}

bool MQTTBackendHost::publish(const char *topic, const char *payload, size_t length, uint8_t qos, bool retain) {
  if (qos != 0) {
    // Host backend currently supports QoS 0 only.
    return false;
  }
  if (this->socket_ == nullptr)
    return false;
  bool ok = this->send_publish_(topic, payload, length, retain);
  this->last_io_ms_ = millis();
  return ok;
}

void MQTTBackendHost::loop() {
  if (this->socket_ == nullptr) {
    return;
  }

  // Read available bytes (non-blocking).
  uint8_t buf[1024];
  while (true) {
    ssize_t n = this->socket_->read(buf, sizeof(buf));
    if (n > 0) {
      this->rx_buffer_.insert(this->rx_buffer_.end(), buf, buf + n);
      this->last_io_ms_ = millis();
      continue;
    }
    if (n == 0) {
      this->handle_disconnect_(MQTTClientDisconnectReason::TCP_DISCONNECTED);
      return;
    }
    // n < 0
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      break;
    }
    this->handle_disconnect_(MQTTClientDisconnectReason::TCP_DISCONNECTED);
    return;
  }

  this->process_rx_();

  // Keepalive (best-effort).
  if (this->connected_ && this->keep_alive_s_ != 0) {
    const uint32_t now = millis();
    const uint32_t interval_ms = static_cast<uint32_t>(this->keep_alive_s_) * 1000UL / 2UL;
    if (interval_ms != 0 && now - this->last_io_ms_ > interval_ms) {
      (void) this->send_pingreq_();
      this->last_io_ms_ = now;
    }
  }
}

void MQTTBackendHost::append_u16_(std::vector<uint8_t> &out, uint16_t value) {
  out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  out.push_back(static_cast<uint8_t>(value & 0xFF));
}

void MQTTBackendHost::append_mqtt_string_(std::vector<uint8_t> &out, const std::string &value) {
  append_u16_(out, static_cast<uint16_t>(value.size()));
  out.insert(out.end(), value.begin(), value.end());
}

void MQTTBackendHost::append_remaining_length_(std::vector<uint8_t> &out, size_t value) {
  do {
    uint8_t encoded = value % 128;
    value /= 128;
    if (value > 0)
      encoded |= 0x80;
    out.push_back(encoded);
  } while (value > 0);
}

bool MQTTBackendHost::decode_remaining_length_(const uint8_t *data, size_t len, size_t *value, size_t *used) {
  size_t multiplier = 1;
  size_t v = 0;
  size_t i = 0;
  uint8_t encoded;
  do {
    if (i >= len)
      return false;
    encoded = data[i++];
    v += (encoded & 127U) * multiplier;
    multiplier *= 128U;
    if (multiplier > 128U * 128U * 128U * 128U)
      return false;
  } while ((encoded & 128U) != 0);
  *value = v;
  *used = i;
  return true;
}

bool MQTTBackendHost::send_packet_(const uint8_t *data, size_t len) {
  if (this->socket_ == nullptr)
    return false;
  size_t offset = 0;
  while (offset < len) {
    ssize_t w = this->socket_->write(data + offset, len - offset);
    if (w > 0) {
      offset += static_cast<size_t>(w);
      continue;
    }
    if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      // Busy; try later.
      return false;
    }
    return false;
  }
  return true;
}

bool MQTTBackendHost::send_connect_() {
  std::vector<uint8_t> vh_payload;
  vh_payload.reserve(128);

  // Variable header
  append_mqtt_string_(vh_payload, "MQTT");
  vh_payload.push_back(0x04);  // Protocol level 4 (3.1.1)

  uint8_t connect_flags = 0;
  if (!this->username_.empty())
    connect_flags |= 0x80;
  if (!this->password_.empty())
    connect_flags |= 0x40;
  if (!this->will_topic_.empty()) {
    connect_flags |= 0x04;  // will flag
    connect_flags |= (this->will_qos_ & 0x03) << 3;
    if (this->will_retain_)
      connect_flags |= 0x20;
  }
  if (this->clean_session_)
    connect_flags |= 0x02;
  vh_payload.push_back(connect_flags);

  append_u16_(vh_payload, this->keep_alive_s_);

  // Payload
  append_mqtt_string_(vh_payload, this->client_id_);
  if (!this->will_topic_.empty()) {
    append_mqtt_string_(vh_payload, this->will_topic_);
    append_mqtt_string_(vh_payload, this->will_payload_);
  }
  if (!this->username_.empty())
    append_mqtt_string_(vh_payload, this->username_);
  if (!this->password_.empty())
    append_mqtt_string_(vh_payload, this->password_);

  std::vector<uint8_t> pkt;
  pkt.reserve(2 + vh_payload.size());
  pkt.push_back(0x10);  // CONNECT
  append_remaining_length_(pkt, vh_payload.size());
  pkt.insert(pkt.end(), vh_payload.begin(), vh_payload.end());

  return this->send_packet_(pkt.data(), pkt.size());
}

bool MQTTBackendHost::send_pingreq_() {
  const uint8_t pkt[2] = {0xC0, 0x00};
  return this->send_packet_(pkt, sizeof(pkt));
}

bool MQTTBackendHost::send_disconnect_() {
  const uint8_t pkt[2] = {0xE0, 0x00};
  return this->send_packet_(pkt, sizeof(pkt));
}

bool MQTTBackendHost::send_subscribe_(uint16_t packet_id, const char *topic, uint8_t qos) {
  if (topic == nullptr)
    return false;
  std::vector<uint8_t> payload;
  payload.reserve(8 + strlen(topic));
  append_u16_(payload, packet_id);
  std::string topic_s(topic);
  append_mqtt_string_(payload, topic_s);
  payload.push_back(qos);

  std::vector<uint8_t> pkt;
  pkt.reserve(2 + payload.size());
  pkt.push_back(0x82);  // SUBSCRIBE (QoS 1 required by spec)
  append_remaining_length_(pkt, payload.size());
  pkt.insert(pkt.end(), payload.begin(), payload.end());
  return this->send_packet_(pkt.data(), pkt.size());
}

bool MQTTBackendHost::send_unsubscribe_(uint16_t packet_id, const char *topic) {
  if (topic == nullptr)
    return false;
  std::vector<uint8_t> payload;
  payload.reserve(8 + strlen(topic));
  append_u16_(payload, packet_id);
  std::string topic_s(topic);
  append_mqtt_string_(payload, topic_s);

  std::vector<uint8_t> pkt;
  pkt.reserve(2 + payload.size());
  pkt.push_back(0xA2);  // UNSUBSCRIBE (QoS 1 required by spec)
  append_remaining_length_(pkt, payload.size());
  pkt.insert(pkt.end(), payload.begin(), payload.end());
  return this->send_packet_(pkt.data(), pkt.size());
}

bool MQTTBackendHost::send_publish_(const char *topic, const char *payload, size_t length, bool retain) {
  if (topic == nullptr || payload == nullptr)
    return false;
  std::vector<uint8_t> vh_payload;
  std::string topic_s(topic);
  append_mqtt_string_(vh_payload, topic_s);
  vh_payload.insert(vh_payload.end(), payload, payload + length);

  std::vector<uint8_t> pkt;
  pkt.reserve(2 + vh_payload.size());
  uint8_t header = 0x30;  // PUBLISH QoS0
  if (retain)
    header |= 0x01;
  pkt.push_back(header);
  append_remaining_length_(pkt, vh_payload.size());
  pkt.insert(pkt.end(), vh_payload.begin(), vh_payload.end());
  return this->send_packet_(pkt.data(), pkt.size());
}

bool MQTTBackendHost::resolve_host_ipv4_(struct sockaddr_in *out) const {
  if (out == nullptr)
    return false;
  struct addrinfo hints {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res = nullptr;
  int err = getaddrinfo(this->server_host_.c_str(), nullptr, &hints, &res);
  if (err != 0 || res == nullptr)
    return false;
  auto *addr_in = reinterpret_cast<struct sockaddr_in *>(res->ai_addr);
  out->sin_addr = addr_in->sin_addr;
  freeaddrinfo(res);
  return true;
}

void MQTTBackendHost::process_rx_() {
  while (true) {
    if (this->rx_buffer_.size() < 2)
      return;

    size_t remaining_len = 0;
    size_t used = 0;
    if (!decode_remaining_length_(this->rx_buffer_.data() + 1, this->rx_buffer_.size() - 1, &remaining_len, &used))
      return;

    const size_t header_len = 1 + used;
    const size_t packet_len = header_len + remaining_len;
    if (this->rx_buffer_.size() < packet_len)
      return;

    const uint8_t type = this->rx_buffer_[0] >> 4;
    const uint8_t flags = this->rx_buffer_[0] & 0x0F;
    const uint8_t *payload = this->rx_buffer_.data() + header_len;
    const size_t payload_len = remaining_len;

    if (type == 2) {  // CONNACK
      if (payload_len >= 2) {
        const bool session_present = (payload[0] & 0x01) != 0;
        const uint8_t rc = payload[1];
        if (rc == 0) {
          this->connected_ = true;
          if (this->on_connect_) {
            this->on_connect_(session_present);
          }
        } else {
          MQTTClientDisconnectReason reason = MQTTClientDisconnectReason::TCP_DISCONNECTED;
          switch (rc) {
            case 1:
              reason = MQTTClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION;
              break;
            case 2:
              reason = MQTTClientDisconnectReason::MQTT_IDENTIFIER_REJECTED;
              break;
            case 3:
              reason = MQTTClientDisconnectReason::MQTT_SERVER_UNAVAILABLE;
              break;
            case 4:
              reason = MQTTClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS;
              break;
            case 5:
              reason = MQTTClientDisconnectReason::MQTT_NOT_AUTHORIZED;
              break;
            default:
              reason = MQTTClientDisconnectReason::TCP_DISCONNECTED;
              break;
          }
          this->handle_disconnect_(reason);
          return;
        }
      }
    } else if (type == 3) {  // PUBLISH
      // Only QoS0 supported; ignore DUP/QoS flags for now.
      (void) flags;
      if (payload_len >= 2) {
        const uint16_t topic_len = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
        if (payload_len >= 2 + topic_len) {
          const char *topic_ptr = reinterpret_cast<const char *>(payload + 2);
          const size_t payload_offset = 2 + topic_len;
          const char *payload_ptr = reinterpret_cast<const char *>(payload + payload_offset);
          const size_t msg_len = payload_len - payload_offset;
          if (this->on_message_) {
            // Single-chunk delivery.
            std::string topic(topic_ptr, topic_len);
            this->on_message_(topic.c_str(), payload_ptr, msg_len, 0, msg_len);
          }
        }
      }
    } else if (type == 9) {  // SUBACK
      if (payload_len >= 3 && this->on_subscribe_) {
        const uint16_t packet_id = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
        this->on_subscribe_(packet_id, payload[2]);
      }
    } else if (type == 11) {  // UNSUBACK
      if (payload_len >= 2 && this->on_unsubscribe_) {
        const uint16_t packet_id = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
        this->on_unsubscribe_(packet_id);
      }
    } else if (type == 13) {  // PINGRESP
      // no-op
    } else if (type == 4) {  // PUBACK
      if (payload_len >= 2 && this->on_publish_) {
        const uint16_t packet_id = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
        this->on_publish_(packet_id);
      }
    }

    this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + packet_len);
  }
}

}  // namespace esphome::mqtt

#endif  // USE_HOST
#endif  // USE_MQTT
