#pragma once
#include "esphome/core/defines.h"
#if defined(USE_NETWORK) && !defined(USE_ZEPHYR)
#include "esphome/components/button/button.h"
#include "esphome/components/socket/socket.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::wake_on_lan {

class WakeOnLanButton final : public button::Button, public Component {
 public:
  void set_macaddr(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f);

  void dump_config() override;
  void setup() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  std::unique_ptr<socket::UDPSendSocket> broadcast_socket_{};
  void press_action() override;
  uint16_t port_{9};
  uint8_t macaddr_[MAC_ADDRESS_SIZE];
};

}  // namespace esphome::wake_on_lan

#endif  // USE_NETWORK && !USE_ZEPHYR
