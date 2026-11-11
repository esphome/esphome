#pragma once

#include "esphome/components/socket/tcp_client_link.h"
#include "esphome/core/component.h"

namespace esphome::tcp_client_link_test_component {

/// Echoes every byte the link receives back to the peer and logs link edges.
class TcpClientLinkTestComponent : public Component {
 public:
  void set_host(const char *host) { this->link_.set_host(host); }
  void set_port(uint16_t port) { this->link_.set_port(port); }
  void set_reconnect_interval(uint32_t ms) { this->link_.set_reconnect_interval(ms); }

  void setup() override;
  void loop() override;
  void on_shutdown() override { this->link_.close(); }

 protected:
  socket::TcpClientLink link_;
  bool was_up_{false};
};

}  // namespace esphome::tcp_client_link_test_component
