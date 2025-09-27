#pragma once
#ifdef USE_ESP_IDF

#include <memory>
#include "esphome/core/helpers.h"
#include "esphome/components/network/ip_address.h"
#include "esphome/components/socket/socket.h"

namespace esphome::captive_portal {

class DNSServer {
 public:
  void start(const network::IPAddress &ip);
  void stop();
  void process_next_request();

 protected:
  void process_dns_request();

  std::unique_ptr<socket::Socket> socket_{nullptr};
  network::IPAddress server_ip_;
  uint8_t buffer_[256];  // DNS_MAX_LEN
};

}  // namespace esphome::captive_portal

#endif  // USE_ESP_IDF
