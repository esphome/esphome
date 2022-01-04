#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/button/button.h"
#include "esphome/core/component.h"
#include "WiFiUdp.h"

namespace esphome {
namespace wake_on_lan {

class WakeOnLanButton : public button::Button {
 public:
  void set_macaddr(const uint8_t* addr); 
 protected:
  WiFiUDP udp_client_{};
  void press_action() override;
  uint8_t macaddr[6] = {0, 0, 0, 0, 0};
};

}
}

#endif