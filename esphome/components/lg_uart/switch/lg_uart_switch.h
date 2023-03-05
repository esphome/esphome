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
  void setup() override;

  void update() override;

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::LATE; }

  std::string describe() override;

  /** Called when uart packet for us inbound */
  void on_reply_packet(std::vector<uint8_t> *pkt) override;

  /** Command specific */
  void set_cmd(const std::string &cmd_str);

 protected:
  void write_state(bool state) override;
};

}  // namespace lg_uart
}  // namespace esphome
