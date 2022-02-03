#include "mqtt_backend.h"

#ifdef USE_MQTT

#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace mqtt {

static const char *TAG = "mqtt.backend";

ErrorCode util::ConnectionEstablisher::init(const std::string &host, uint16_t port, uint32_t timeout) {
  if (state_ != State::UNINIT) {
    enter_error_();
    ESP_LOGD(TAG, "conn init bad state");
    return ErrorCode::BAD_STATE;
  }
  struct addrinfo hints;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  std::string port_s = to_string(port);
  getaddrinfo_ = socket::getaddrinfo_async(host.c_str(), port_s.c_str(), &hints);
  if (!getaddrinfo_) {
    enter_error_();
    return ErrorCode::RESOLVE_ERROR;
  }
  start_ = millis();
  timeout_ = timeout;
  state_ = State::RESOLVING;
  return ErrorCode::OK;
}

void util::ConnectionEstablisher::enter_error_() {
  state_ = State::ERROR;
  getaddrinfo_.reset();
  socket_.reset();
}

ErrorCode util::ConnectionEstablisher::loop() {
  ErrorCode ec;
  uint32_t now = millis();

  switch (state_) {
    case State::UNINIT: {
      enter_error_();
      state_ = State::ERROR;
      ESP_LOGD(TAG, "conn uninit bad state");
      return ErrorCode::BAD_STATE;
    }

    case State::RESOLVING: {
      if (getaddrinfo_->completed()) {
        struct addrinfo *res;
        int r = getaddrinfo_->fetch_result(&res);
        if (r != 0) {
          enter_error_();
          ESP_LOGW(TAG, "Address resolve failed with error %s", gai_strerror(r));
          return ErrorCode::RESOLVE_ERROR;
        }
        if (res == nullptr) {
          enter_error_();
          ESP_LOGW(TAG, "Address resolve returned no results");
          return ErrorCode::RESOLVE_ERROR;
        }

        ESP_LOGD(TAG, "address resolved!");

        socket_ = socket::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (!socket_) {
          freeaddrinfo(res);
          enter_error_();
          ESP_LOGW(TAG, "Socket creation failed with error %s", strerror(errno));
          return ErrorCode::SOCKET_ERROR;
        }

        r = socket_->setblocking(false);
        if (r != 0) {
          enter_error_();
          ESP_LOGV(TAG, "Setting nonblocking socket failed with error %s", strerror(errno));
          return ErrorCode::SOCKET_ERROR;
        }

        r = socket_->connect(res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);

        if (r == 0) {
          // connection established immediately
          getaddrinfo_.reset();
          state_ = State::CONNECTED;
          return ErrorCode::OK;
        } else if (errno == EINPROGRESS) {
          getaddrinfo_.reset();
          state_ = State::CONNECTING;
        } else {
          enter_error_();
          ESP_LOGW(TAG, "Socket connect failed with error %s", strerror(errno));
          return ErrorCode::SOCKET_ERROR;
        }
      }

      if (now - start_ >= timeout_) {
        enter_error_();
        ESP_LOGW(TAG, "Timeout resolving address");
        return ErrorCode::TIMEOUT;
      }

      return ErrorCode::IN_PROGRESS;
    }

    case State::CONNECTING: {
      int r = socket_->connect_finished();
      if (r == 0) {
        // connection established
        state_ = State::CONNECTED;
        return ErrorCode::OK;
      } else if (errno == EINPROGRESS) {
        // not established yet

        if (now - start_ >= timeout_) {
          enter_error_();
          ESP_LOGW(TAG, "Timeout connecting to address");
          return ErrorCode::TIMEOUT;
        }

        return ErrorCode::IN_PROGRESS;
      } else {
        enter_error_();
        ESP_LOGW(TAG, "Socket connect failed with error %s", strerror(errno));
        return ErrorCode::SOCKET_ERROR;
      }
    }

    case State::CONNECTED: {
      return ErrorCode::OK;
    }

    case State::FINISHED:
    case State::ERROR:
    default: {
      return ErrorCode::BAD_STATE;
    }
  }
}

std::unique_ptr<socket::Socket> util::ConnectionEstablisher::extract_socket() {
  if (state_ != State::CONNECTED)
    return nullptr;
  state_ = State::FINISHED;
  return std::move(socket_);
}

ErrorCode util::BufferedWriter::write(const std::unique_ptr<socket::Socket> &sock, const uint8_t *data, size_t len, bool do_buffer) {
  if (len == 0)
    return ErrorCode::OK;
  ErrorCode ec;

  if (!tx_buf_.empty()) {
    // try to empty tx_buf_ first
    ec = try_drain(sock);
    if (ec != ErrorCode::OK && ec != ErrorCode::WOULD_BLOCK)
      return ec;
  }

  if (!tx_buf_.empty()) {
    // tx buf not empty, can't write now because then stream would be inconsistent
    if (!do_buffer)
      return ErrorCode::WOULD_BLOCK;

    tx_buf_.insert(tx_buf_.end(), data, data + len);
    return ErrorCode::OK;
  }

  ssize_t sent = sock->write(data, len);
  if (sent == 0 || (sent == -1 && (errno == EWOULDBLOCK || errno == EAGAIN))) {
    // operation would block, add to tx_buf if buffering
    if (!do_buffer)
      return ErrorCode::WOULD_BLOCK;
    tx_buf_.insert(tx_buf_.end(), data, data + len);
    return ErrorCode::OK;
  } else if (sent == -1) {
    // an error occured
    ESP_LOGV(TAG, "Socket write failed with errno %d", errno);
    return ErrorCode::SOCKET_ERROR;
  } else if ((size_t) sent != len) {
    // partially sent, add end to tx_buf (even if not set to buffering, to prevent
    // partial packet transmission)
    tx_buf_.insert(tx_buf_.end(), data + sent, data + len);
    return ErrorCode::OK;
  }
  // fully sent
  return ErrorCode::OK;
}
ErrorCode util::BufferedWriter::try_drain(const std::unique_ptr<socket::Socket> &sock) {
  // try send from tx_buf
  while (!tx_buf_.empty()) {
    ssize_t sent = sock->write(tx_buf_.data(), tx_buf_.size());
    if (sent == 0 || (sent == -1 && (errno = EWOULDBLOCK || errno == EAGAIN))) {
      break;
    } else if (sent == -1) {
      ESP_LOGV(TAG, "Socket write failed with errno %d", errno);
      return ErrorCode::SOCKET_ERROR;
    }

    // TODO: inefficient if multiple packets in txbuf
    // replace with deque of buffers
    tx_buf_.erase(tx_buf_.begin(), tx_buf_.begin() + sent);
  }

  return ErrorCode::OK;
}

ErrorCode MQTTConnection::init(ConnectParams *params, MQTTSession *session) {
  if (state_ != State::UNINIT) {
    enter_error_();
    return ErrorCode::BAD_STATE;
  }
  params_ = params;
  session_ = session;
  connection_establisher_ = make_unique<util::ConnectionEstablisher>();
  ErrorCode ec = connection_establisher_->init(params_->host, params_->port, 5000);
  if (ec != ErrorCode::OK) {
    enter_error_();
    return ec;
  }

  state_ = State::CONNECTING;
  return ErrorCode::OK;
}

ErrorCode MQTTConnection::loop() {
  ErrorCode ec;

  switch (state_) {
    case State::CONNECTING: {
      ec = connection_establisher_->loop();
      if (ec == ErrorCode::OK) {
        // connection established
        socket_ = connection_establisher_->extract_socket();

        ConnectPacket packet{};
        packet.client_id = params_->client_id;
        packet.username = params_->username;
        packet.password = params_->password;
        packet.will_topic = params_->will_topic;
        packet.will_message = params_->will_message;
        packet.will_qos = params_->will_qos;
        packet.will_retain = params_->will_retain;
        packet.clean_session = true;
        packet.keep_alive = params_->keep_alive;

        std::vector<uint8_t> packet_enc;
        ec = packet.encode(packet_enc);
        if (ec != ErrorCode::OK) {
          enter_error_();
          return ec;
        }

        ec = writer_.write(socket_, packet_enc.data(), packet_enc.size(), true);
        if (ec != ErrorCode::OK) {
          enter_error_();
          return ec;
        }

        state_ = State::WAIT_CONNACK;
        connection_establisher_.reset();
      } else if (ec != ErrorCode::IN_PROGRESS) {
        enter_error_();
        return ec;
      }
      return ErrorCode::OK;
    }

    case State::WAIT_CONNACK:
    case State::CONNECTED: {
      ec = writer_.try_drain(socket_);
      if (ec != ErrorCode::OK) {
        enter_error_();
        return ec;
      }

      ec = read_packet_();
      if (ec != ErrorCode::OK && ec != ErrorCode::WOULD_BLOCK) {
        enter_error_();
        return ec;
      }

      return ErrorCode::OK;
    }

    case State::UNINIT:
    case State::DISCONNECTED:
    case State::ERROR:
    default: {
      enter_error_();
      ESP_LOGD(TAG, "bad state %d", (int) state_);
      return ErrorCode::BAD_STATE;
    }
  }
}

void MQTTConnection::enter_error_() {
  ESP_LOGD(TAG, "enter_error");
  connection_establisher_.reset();
  socket_.reset();
  rx_header_buf_ = {};
  rx_buf_ = {};
  writer_.stop();
  state_ = State::ERROR;
}

ErrorCode MQTTConnection::read_packet_() {
  if (state_ != State::CONNECTED && state_ != State::WAIT_CONNACK) {
    enter_error_();
    ESP_LOGD(TAG, "read_packet_ bad state");
    return ErrorCode::BAD_STATE;
  }
  ErrorCode ec;

  if (!rx_header_parsed_) {
    while (true) {
      uint8_t v;
      ssize_t received = socket_->read(&v, 1);
      if (received == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
        // would block
        return ErrorCode::WOULD_BLOCK;
      } else if (received == -1) {
        // error
        enter_error_();
        ESP_LOGV(TAG, "Socket read failed with errno %d", errno);
        return ErrorCode::SOCKET_ERROR;
      } else if (received == 0) {
        // EOF
        enter_error_();
        ESP_LOGV(TAG, "Socket EOF");
        return ErrorCode::SOCKET_ERROR;
      }
      rx_header_buf_.push_back(v);

      // try parse buf
      if (rx_header_buf_.size() == 1)
        continue;

      rx_header_parsed_type_ = (rx_header_buf_[0] >> 4) & 0x0F;
      rx_header_parsed_flags_ = (rx_header_buf_[0] >> 0) & 0x0F;

      size_t multiplier = 1, value = 0;
      size_t i = 1;
      uint8_t enc;
      bool parsed = true;
      do {
        if (i >= rx_header_buf_.size()) {
          // not enough data yet
          parsed = false;
          break;
        }
        enc = rx_header_buf_[i];
        value += (enc & 0x7F) * multiplier;
        multiplier <<= 7;
      } while (enc & 0x80);
      if (!parsed)
        continue;

      rx_header_parsed_ = true;
      rx_header_parsed_len_ = value;
    }
  }
  // header reading done

  // reserve space for body
  if (rx_buf_.size() != rx_header_parsed_len_) {
    rx_buf_.resize(rx_header_parsed_len_);
  }

  if (rx_buf_len_ < rx_header_parsed_len_) {
    // more data to read
    size_t to_read = rx_header_parsed_len_ - rx_buf_len_;
    ssize_t received = socket_->read(&rx_buf_[rx_buf_len_], to_read);
    if (received == -1) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        return ErrorCode::WOULD_BLOCK;
      }
      enter_error_();
      ESP_LOGV(TAG, "Socket read failed with errno %d", errno);
      return ErrorCode::SOCKET_ERROR;
    } else if (received == 0) {
      enter_error_();
      ESP_LOGD(TAG, "Connection closed");
      return ErrorCode::CONNECTION_CLOSED;
    }
    rx_buf_len_ += received;
    if ((size_t) received != to_read) {
      // not all read
      return ErrorCode::WOULD_BLOCK;
    }
  }
  // body reading done

  ec = handle_packet_(rx_header_parsed_type_, rx_header_parsed_flags_, rx_buf_.data(), rx_buf_.size());
  // prepare for next packet
  rx_header_parsed_ = false;
  return ec;
}

ErrorCode MQTTConnection::handle_packet_(uint8_t packet_type, uint8_t flags, const uint8_t *data, size_t len) {
  util::Parser parser(data, len);
  ErrorCode ec;

  switch (static_cast<PacketType>(packet_type)) {
    case PacketType::CONNACK: {
      if (state_ != State::WAIT_CONNACK) {
        enter_error_();
        ESP_LOGV(TAG, "Bad state for connack %d", (int) state_);
        return ErrorCode::BAD_STATE;
      }
      ConnackPacket packet{};
      ec = packet.decode(flags, parser);
      if (ec != ErrorCode::OK) {
        enter_error_();
        ESP_LOGV(TAG, "Error decoding connack packet %d", (int) ec);
        return ec;
      }

      if (packet.connect_return_code != ConnectReturnCode::ACCEPTED) {
        const char *reason;
        switch (packet.connect_return_code) {
          case ConnectReturnCode::UNACCEPTABLE_PROTOCOL_VERSION:
            reason = "unacceptable protocol version";
            break;
          case ConnectReturnCode::IDENTIFIER_REJECTED:
            reason = "identifier rejected";
            break;
          case ConnectReturnCode::SERVER_UNAVAILABLE:
            reason = "server unavailable";
            break;
          case ConnectReturnCode::BAD_USER_NAME_OR_PASSWORD:
            reason = "bad user name or password";
            break;
          case ConnectReturnCode::NOT_AUTHORIZED:
            reason = "not authorized";
            break;
          default:
            reason = "unknown";
            break;
        }
        enter_error_();
        ESP_LOGW(TAG, "Connect failed: %s", reason);
        return ErrorCode::PROTOCOL_ERROR;
      }

      ESP_LOGD(TAG, "Connected!");
      state_ = State::CONNECTED;
      return ErrorCode::OK;
    }
    case PacketType::PUBLISH: {
      if (state_ != State::CONNECTED) {
        enter_error_();
        ESP_LOGV(TAG, "Bad state for publish %d", (int) state_);
        return ErrorCode::BAD_STATE;
      }

      return ErrorCode::OK;
    }

    case PacketType::PUBACK:
    case PacketType::PUBREC:
    case PacketType::PUBREL:
    case PacketType::PUBCOMP:
    case PacketType::SUBACK:
    case PacketType::UNSUBACK: {
      // TODO
      ESP_LOGD(TAG, "Received packet with type %u", packet_type);
      return ErrorCode::OK;
    }

    case PacketType::PINGRESP: {
      // TODO rx timer
      return ErrorCode::OK;
    }

    case PacketType::CONNECT:
    case PacketType::DISCONNECT:
    case PacketType::SUBSCRIBE:
    case PacketType::UNSUBSCRIBE:
    case PacketType::PINGREQ:
    default: {
      enter_error_();
      ESP_LOGW(TAG, "Received unknown packet type %u", packet_type);
      return ErrorCode::UNEXPECTED;
    }

  }
}

ErrorCode MQTTConnection::publish(std::string topic, std::vector<uint8_t> message, bool retain, QOSLevel qos) {
  PublishPacket packet{};
  packet.topic = std::move(topic);
  packet.message = std::move(message);
  packet.retain = retain;
  packet.qos = qos;
  if (packet.qos != QOSLevel::QOS0) {
    packet.packet_identifier = session_->create_packet_id();
  }
  packet.dup = false;

  std::vector<uint8_t> packet_enc;
  ErrorCode ec = packet.encode(packet_enc);
  if (ec != ErrorCode::OK)
    return ec;

  ec = writer_.write(socket_, packet_enc.data(), packet_enc.size(), qos != QOSLevel::QOS0);
  if (ec != ErrorCode::OK && ec != ErrorCode::WOULD_BLOCK) {
    enter_error_();
    ESP_LOGV(TAG, "publish write failed");
    return ec;
  }

  return ec;
}
ErrorCode MQTTConnection::subscribe(std::vector<Subscription> subscriptions) {
  SubscribePacket packet{};
  packet.subscriptions = std::move(subscriptions);
  packet.packet_identifier = session_->create_packet_id();

  std::vector<uint8_t> packet_enc;
  ErrorCode ec = packet.encode(packet_enc);
  if (ec != ErrorCode::OK)
    return ec;

  ec = writer_.write(socket_, packet_enc.data(), packet_enc.size(), true);
  if (ec != ErrorCode::OK) {
    enter_error_();
    ESP_LOGV(TAG, "subscribe write failed");
    return ec;
  }
  return ec;
}
ErrorCode MQTTConnection::unsubscribe(std::vector<std::string> topic_filters) {
  UnsubscribePacket packet{};
  packet.topic_filters = std::move(topic_filters);
  packet.packet_identifier = session_->create_packet_id();

  std::vector<uint8_t> packet_enc;
  ErrorCode ec = packet.encode(packet_enc);
  if (ec != ErrorCode::OK)
    return ec;

  ec = writer_.write(socket_, packet_enc.data(), packet_enc.size(), true);
  if (ec != ErrorCode::OK) {
    enter_error_();
    ESP_LOGV(TAG, "unsubscribe write failed");
    return ec;
  }
  return ec;
}

}  // namespace mqtt
}  // namespace esphome

#endif  // USE_MQTT
