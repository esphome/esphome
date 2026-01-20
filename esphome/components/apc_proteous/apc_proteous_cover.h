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

  std::string rx_buffer_;
  bool query_s_next_{true};
  uint8_t s_status_{0};
  uint8_t x_status_{0};
  bool initial_state_received_{false};
  float target_position_{0};
};

}  // namespace apc_proteous
}  // namespace esphome
