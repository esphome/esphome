#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/components/lg_uart/lg_hub.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace lg_uart {

class LGUartNumber : public number::Number, public LGUartClient, public PollingComponent {
 public:
  void update() override;

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::LATE; }

  std::string describe() override;

  void set_encoding_base(int base) override { this->encoding_base_ = base; };

  int get_encoding_base() override { return this->encoding_base_; };

  /** Called when uart packet for us inbound */
  void on_reply_packet(uint8_t pkt[]) override;

  /** Command specific */
  void set_cmd(const std::string &cmd_str) {
    this->cmd_str_[0] = cmd_str[0];
    this->cmd_str_[1] = cmd_str[1];
  }

 protected:
  // Two chars + termination
  char cmd_str_[3] = {0};
  uint8_t reply_[PACKET_LEN] = {0, 0, 0};

  // Keep track of base 16 or base 10 decoding on reply packets
  int encoding_base_ = 0;

  // Called when user has new value
  void control(float value) override;
};

}  // namespace lg_uart
}  // namespace esphome
