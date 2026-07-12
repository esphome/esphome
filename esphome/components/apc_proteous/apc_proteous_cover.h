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
  void open_cmd_();
  void close_cmd_();
  void stop_cmd_();
  void send_command_(const char *cmd);
  void write_command_(const char *cmd);
  void retry_pending_command_();
  void control(const cover::CoverCall &call) override;
  void parse_response_();

  // Debug aid: start logging all UART traffic (commands, queries and every received
  // frame) after a command is sent, until the gate returns to idle having moved, or
  // TRACE_MAX_MS elapses (so a dropped command that never moves it does not trace
  // forever). Used to diagnose unacknowledged commands and bad position reports.
  void start_trace_(const char *what);

  // The published state (current_operation, position) is updated only from status
  // reads, so it always reflects what the controller reports. pending_command_ is a
  // private record of the last movement command we sent; it lets a stop cancel an
  // in-flight command before the controller has reported the resulting motion, and
  // lets us resend a command the controller dropped (the serial link occasionally
  // loses one). It is not published, and is only treated as in-flight for
  // PENDING_COMMAND_MS.
  static const uint32_t PENDING_COMMAND_MS = 3000;

  // A movement command is acknowledged once the controller reports motion in the
  // commanded direction. If it has not within COMMAND_ACK_MS, resend it, up to
  // MAX_COMMAND_RETRIES times. Only directional OPEN/CLOSE are retried; the stop
  // toggle is never resent, since a spurious toggle could start the gate.
  static const uint32_t COMMAND_ACK_MS = 2000;
  static const uint8_t MAX_COMMAND_RETRIES = 3;

  // The controller echoes every command it receives, sometimes without a terminator. A query
  // sent too soon after a command collides with that echo, producing a corrupt frame and a
  // dropped command. After sending a command, hold off polling for COMMAND_QUIET_MS so the
  // echo drains and the command takes effect before the next query.
  static const uint32_t COMMAND_QUIET_MS = 250;

  // Safety cap on the UART trace so it always stops even if the gate never moves.
  static const uint32_t TRACE_MAX_MS = 60000;

  std::string rx_buffer_;
  bool query_s_next_{true};
  uint8_t s_status_{0};
  uint8_t x_status_{0};
  float target_position_{0};
  const char *pending_command_{nullptr};
  uint32_t pending_command_time_{0};
  uint32_t last_command_tx_{0};
  uint8_t command_retries_{0};
  bool trace_active_{false};
  bool trace_saw_motion_{false};
  uint32_t trace_start_time_{0};
};

}  // namespace esphome::apc_proteous
