#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/components/lg_uart/lg_hub.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace lg_uart {
class LGUartSwitch : public switch_::Switch, public LGUartClient, public PollingComponent {
 public:
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  /** Command specific */
  void set_cmd(const std::string cmd_str) {
    this->cmd_str_[0] = cmd_str[0];
    this->cmd_str_[1] = cmd_str[1];
  }

 protected:
  void write_state(bool state) override;

  char cmd_str_[2];
  uint8_t reply[PACKET_LEN];

 private:
};

}  // namespace lg_uart
}  // namespace esphome
