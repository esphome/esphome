#pragma once

#include "esphome/components/button/button.h"
#include "esphome/core/component.h"

namespace esphome {
namespace wake_on_lan {

class WakeOnLanButton : public button::Button, public Component {
 public:
  void set_macaddr(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f);

  void dump_config() override;

 protected:
  void press_action() override;
  uint8_t macaddr_[6] = {0, 0, 0, 0, 0};
  void fill_buffer_(uint8_t *buff);
  void fill_preamble_(uint8_t *buff);
  void fill_mac_address_(uint8_t *buff);
  // void fill_password(uint8_t *buff); for future implementation
};

}  // namespace wake_on_lan
}  // namespace esphome
