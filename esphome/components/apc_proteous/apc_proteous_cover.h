#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/cover/cover.h"

namespace esphome::apc_proteous {

class APCProteousCover : public cover::Cover, public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  cover::CoverTraits get_traits() override;

 protected:
  void control(const cover::CoverCall &call) override;
  void parse_response_();
  void send_command_(const char *cmd);
  void write_command_(const char *cmd, uint32_t now);
  void stop_cmd_(uint32_t now);
  void retry_pending_command_(uint32_t now);
  void clear_pending_();

  // The longest valid frame is "?s=XX"; anything longer is line noise
  static constexpr size_t MAX_RESPONSE_LEN = 16;

  // A movement command is acknowledged once the controller reports motion in the commanded
  // direction; resend it this often, up to MAX_COMMAND_RETRIES times, since the serial link
  // occasionally drops one. The stop toggle is never resent because a spurious toggle could start the gate.
  static constexpr uint32_t COMMAND_ACK_MS = 2000;
  static constexpr uint8_t MAX_COMMAND_RETRIES = 3;

  // The controller echoes every command, sometimes without a terminator. A query sent too soon
  // after a command collides with that echo, so polling pauses for this long after each command.
  static constexpr uint32_t COMMAND_QUIET_MS = 250;

  // Set only for a partial target; the gate is stopped when the reported position reaches it
  optional<float> target_position_{};
  // Last movement command sent and not yet acknowledged; lets a stop cancel it and drives the retries
  const char *pending_command_{nullptr};
  uint32_t pending_command_time_{0};
  uint32_t last_command_tx_{0};
  char rx_buffer_[MAX_RESPONSE_LEN + 1];
  uint8_t rx_len_{0};
  uint8_t command_retries_{0};
  bool query_s_next_{true};
};

}  // namespace esphome::apc_proteous
