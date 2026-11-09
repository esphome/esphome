#pragma once
#include "esphome/core/defines.h"
// ESP32 backend for CaptiveDNS: answers every A query with the access point address.
#if defined(USE_ESP32) && (defined(USE_CAPTIVE_PORTAL) || defined(USE_WEBSERVER_CAPTIVE))

#include "esphome/core/helpers.h"
#include "esphome/components/network/ip_address.h"
#include "esphome/components/socket/socket.h"

namespace esphome::web_server_base {

class DNSServer {
 public:
  /// Returns false when the socket could not be created or bound.
  bool start(const network::IPAddress &ip);
  void stop();
  void process_next_request();

 protected:
  // No explicit close() needed — listen sockets have no active connections on
  // failure/shutdown. Destructor handles fd cleanup (close or abort per platform).
  inline void destroy_socket_() {
    delete this->socket_;
    this->socket_ = nullptr;
  }
  static constexpr size_t DNS_BUFFER_SIZE = 192;

  socket::ListenSocket *socket_{nullptr};
  network::IPAddress server_ip_;
  uint8_t buffer_[DNS_BUFFER_SIZE];
};

}  // namespace esphome::web_server_base

#endif  // USE_ESP32 && (USE_CAPTIVE_PORTAL || USE_WEBSERVER_CAPTIVE)
