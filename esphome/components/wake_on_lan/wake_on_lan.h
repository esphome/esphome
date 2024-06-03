#pragma once

#include "esphome/components/button/button.h"
#include "esphome/core/component.h"
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
#include "esphome/components/socket/socket.h"
#else
#include "WiFiUdp.h"
#endif

namespace esphome {
namespace wake_on_lan {

class WakeOnLanButton : public button::Button, public Component {
 public:
  void set_macaddr(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f);

  void dump_config() override;
  void setup() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  std::unique_ptr<socket::Socket> broadcast_socket_{};
#else
  WiFiUDP udp_client_{};
#endif
  void press_action() override;
  uint16_t port_{9};
  uint8_t macaddr_[6];
};

}  // namespace wake_on_lan
}  // namespace esphome
