#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/cover/cover.h"

namespace esphome {
namespace apc_proteous {

class APCProteousCover : public cover::Cover, public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  cover::CoverTraits get_traits() override;

 protected:
  void open_cmd_();
  void close_cmd_();
  void stop_cmd_();
  void control(const cover::CoverCall &call) override;
  void parse_response_();

  // Grace period after a movement command during which a "not operating" status
  // is not allowed to revert the operation to idle. The controller may not report
  // "operating" until the gate physically starts moving.
  static const uint32_t COMMAND_LOCKOUT_MS = 3000;

  std::string rx_buffer_;
  bool query_s_next_{true};
  uint8_t s_status_{0};
  uint8_t x_status_{0};
  bool initial_state_received_{false};
  float target_position_{0};
  uint32_t last_command_time_{0};
};

}  // namespace apc_proteous
}  // namespace esphome
