#pragma once

#include "esphome/components/button/button.h"
#include "esphome/core/component.h"
#include "esphome/components/socket/socket.h"

namespace esphome {
namespace wake_on_lan {

class WakeOnLanButton : public button::Button, public Component {
 public:
  void set_macaddr(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f);

  void dump_config() override;
  void setup() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  void press_action() override;
  std::unique_ptr<socket::Socket> broadcast_socket_ = nullptr;
  uint16_t port_{9};
  uint8_t macaddr_[6];
};

}  // namespace wake_on_lan
}  // namespace esphome
