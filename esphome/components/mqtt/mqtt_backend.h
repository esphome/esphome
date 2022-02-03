#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT

#include "packets.h"
#include "esphome/components/socket/socket.h"
#include "esphome/components/socket/getaddrinfo.h"
#include <memory>
#include <set>

namespace esphome {
namespace mqtt {

namespace util {

class ConnectionEstablisher {
 public:
  ErrorCode init(const std::string &host, uint16_t port, uint32_t timeout);
  ErrorCode loop();
  // Should only be called when loop() returns OK, is guaranteed to succeed
  std::unique_ptr<socket::Socket> extract_socket();

 protected:
  void enter_error_();

  std::unique_ptr<socket::Socket> socket_;
  std::unique_ptr<socket::GetaddrinfoFuture> getaddrinfo_;
  uint32_t timeout_;
  uint32_t start_;

  enum class State {
    UNINIT = 0,
    RESOLVING = 1,
    CONNECTING = 2,
    CONNECTED = 3,
    FINISHED = 4,
    ERROR = 5,
  } state_ = State::UNINIT;
};

class BufferedWriter {
 public:
  ErrorCode write(const std::unique_ptr<socket::Socket> &sock, const uint8_t *data, size_t len,
                  bool do_buffer);
  ErrorCode try_drain(const std::unique_ptr<socket::Socket> &sock);
  void stop() {
    tx_buf_ = {};
  }

 protected:
  std::vector<uint8_t> tx_buf_;
};

}  // namespace util

struct ConnectParams {
  std::string host;
  uint16_t port;

  std::string client_id;
  optional<std::string> username;
  optional<std::vector<uint8_t>> password;
  std::string will_topic;
  std::vector<uint8_t> will_message;
  QOSLevel will_qos;
  bool will_retain;
  uint16_t keep_alive;
};

class MQTTSession {
 public:
  bool get_has_session() const { return has_session_; }
  void set_has_session(bool has_session) { has_session_ = has_session; }
  void clean_session() {
    packet_id_counter_ = 0;
    used_packet_ids_.clear();
  }
  uint16_t create_packet_id() {
    while (true) {
      packet_id_counter_++;
      if (packet_id_counter_ == 0 || used_packet_ids_.count(packet_id_counter_) > 0) {
        continue;
      }
      used_packet_ids_.insert(packet_id_counter_);
      return packet_id_counter_;
    }
  }
  void return_packet_id(uint16_t packet_id) {
    used_packet_ids_.erase(packet_id);
  }
 protected:
  bool has_session_ = false;
  uint32_t packet_id_counter_ = 0;
  std::set<uint16_t> used_packet_ids_;
};

class MQTTConnection {
 public:
  ErrorCode init(ConnectParams *params, MQTTSession *session);
  ErrorCode loop();
  bool is_connected() { return state_ == State::CONNECTED; }

  ErrorCode publish(std::string topic, std::vector<uint8_t> message, bool retain, QOSLevel qos);
  ErrorCode subscribe(std::vector<Subscription> subscriptions);
  ErrorCode unsubscribe(std::vector<std::string> topic_filters);

 protected:
  void enter_error_();
  ErrorCode read_packet_();
  ErrorCode handle_packet_(uint8_t packet_type, uint8_t flags, const uint8_t *data, size_t len);

  ConnectParams *params_;
  MQTTSession *session_;
  std::unique_ptr<util::ConnectionEstablisher> connection_establisher_;
  std::unique_ptr<socket::Socket> socket_;

  std::vector<uint8_t> rx_header_buf_;
  bool rx_header_parsed_ = false;
  uint8_t rx_header_parsed_type_ = 0;
  uint8_t rx_header_parsed_flags_ = 0;
  size_t rx_header_parsed_len_ = 0;

  std::vector<uint8_t> rx_buf_;
  size_t rx_buf_len_ = 0;

  util::BufferedWriter writer_;

  enum class State {
    UNINIT = 0,
    CONNECTING = 1,
    WAIT_CONNACK = 2,
    CONNECTED = 3,
    DISCONNECTED = 4,
    ERROR = 5,
  } state_ = State::UNINIT;
};

}  // namespace mqtt
}  // namespace esphome

#endif  // USE_MQTT
