#pragma once
#if defined(USE_ARDUINO) && (defined(USE_ESP8266) || defined(USE_RP2040) || defined(USE_LIBRETINY))

#include <memory>
#include "dns_server_common.h"
#include "esphome/core/helpers.h"
#include "esphome/components/network/ip_address.h"
#include <WiFiUdp.h>

namespace esphome::captive_portal {

class DNSServer {
 public:
  void start(const network::IPAddress &ip);
  void stop();
  void process_next_request();

 protected:
  std::unique_ptr<WiFiUDP> udp_{nullptr};
  network::IPAddress server_ip_;
  uint8_t buffer_[DNS_BUFFER_SIZE];
};

}  // namespace esphome::captive_portal

#endif  // USE_ARDUINO && (USE_ESP8266 || USE_RP2040 || USE_LIBRETINY)
