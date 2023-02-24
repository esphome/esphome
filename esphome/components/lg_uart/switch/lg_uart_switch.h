#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/components/lg_uart/lg_hub.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace lg_uart {

// static const char *const TAG = "LGUart.switch";

class LGUartSwitch : public switch_::Switch, public LGUartClient, public PollingComponent {
 public:
  void update() override;
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  std::string describe() override;
  void set_encoding_base(int base) override{};
  int get_encoding_base() override { return this->encoding_base_; };

  void on_reply_packet(uint8_t pkt[]) override;

  /** Command specific */
  void set_cmd(const std::string &cmd_str) {
    this->cmd_str_[0] = cmd_str[0];
    this->cmd_str_[1] = cmd_str[1];
  }

 protected:
  void write_state(bool state) override;

  // Two chars + termination
  char cmd_str_[3] = {0};
  uint8_t reply_[PACKET_LEN] = {0};

  // Paramater best represented by a switch should be either 00 or 01 so keep base10
  int encoding_base_ = 10;

 private:
};

}  // namespace lg_uart
}  // namespace esphome
