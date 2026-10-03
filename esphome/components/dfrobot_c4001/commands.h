#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dfrobot_c4001 {
class DFRobotC4001Hub;
// Enumeration for Command States
enum CommandState {
  STATE_CMD_SEND = 0,  // command needs to be sent
  STATE_WAIT_ECHO,     // command was sent, now waiting for command echo
  STATE_PROCESS,       // command in process
  STATE_WAIT_PROMPT,   // waiting for prompt, terminates this transaction
  STATE_DONE,
};

// Use command queue and time stamps to avoid blocking.
// When component has run time, check if minimum time (1s) between
// commands has passed. After that run a command from the queue.
class Command {
 public:
  virtual ~Command() = default;
  virtual uint8_t execute(DFRobotC4001Hub *parent);
  virtual void on_message();
  uint8_t retry_power_stop{false};

 protected:
  DFRobotC4001Hub *parent_{nullptr};
  uint8_t state_{STATE_CMD_SEND};
  uint8_t done_{false};
  uint8_t error_{false};
  std::string cmd_;
  char *read_buffer_{nullptr};
  int8_t error_count_{0};
  int8_t retries_left_{2};
  uint32_t cmd_duration_ms_{10};
  uint32_t timeout_ms_{1500};
};

class ReadStateCommand : public Command {
 public:
  uint8_t execute(DFRobotC4001Hub *parent) override;
};

class PowerCommand : public Command {
 public:
  PowerCommand(bool power_on);
};

class GetRangeCommand : public Command {
 public:
  GetRangeCommand();
  void on_message() override;

 protected:
  optional<float> min_range_;
  optional<float> max_range_;
};

class SetRangeCommand : public Command {
 public:
  SetRangeCommand(float min_range, float max_range);
};

class GetTrigRangeCommand : public Command {
 public:
  GetTrigRangeCommand();
  void on_message() override;

 protected:
  optional<float> trigger_range_;
};

class SetTrigRangeCommand : public Command {
 public:
  SetTrigRangeCommand(float trigger_range);
};

class GetSensitivityCommand : public Command {
 public:
  GetSensitivityCommand();
  void on_message() override;

 protected:
  optional<float> hold_sensitivity_;
  optional<float> trigger_sensitivity_;
};

class SetSensitivityCommand : public Command {
 public:
  SetSensitivityCommand(float hold_sensitivity, float trigger_sensitivity);
};

class GetLatencyCommand : public Command {
 public:
  GetLatencyCommand();
  void on_message() override;

 protected:
  optional<float> on_latency_;
  optional<float> off_latency_;
};

class SetLatencyCommand : public Command {
 public:
  SetLatencyCommand(float on_latency, float off_latency);
};

class GetInhibitTimeCommand : public Command {
 public:
  GetInhibitTimeCommand();
  void on_message() override;

 protected:
  optional<float> inhibit_time_;
};

class SetInhibitTimeCommand : public Command {
 public:
  SetInhibitTimeCommand(float inhibit_time);
};

class GetThrFactorCommand : public Command {
 public:
  GetThrFactorCommand();
  void on_message() override;

 protected:
  optional<float> threshold_factor_;
};

class SetThrFactorCommand : public Command {
 public:
  SetThrFactorCommand(float threshold_factor);
};

class SetUartOutputCommand : public Command {
 public:
  SetUartOutputCommand(bool enable);
};

class SetLedModeCommand : public Command {
 public:
  SetLedModeCommand(bool value);
};

class SetGpioModeCommand : public Command {
 public:
  SetGpioModeCommand(bool value);
};

class GetGpioModeCommand : public Command {
 public:
  GetGpioModeCommand();
  void on_message() override;

 protected:
  optional<uint8_t> value_;
};

class GetMicroMotionCommand : public Command {
 public:
  GetMicroMotionCommand();
  void on_message() override;

 protected:
  optional<bool> micro_motion_;
};

class SetMicroMotionCommand : public Command {
 public:
  SetMicroMotionCommand(bool enable);
  void on_message() override;

 protected:
  bool micro_motion_;
};

class FactoryResetCommand : public Command {
 public:
  FactoryResetCommand();
  void on_message() override;
};

class ResetSystemCommand : public Command {
 public:
  ResetSystemCommand(bool read_config);
  void on_message() override;

 protected:
  bool read_config_;
};

class SaveCfgCommand : public Command {
 public:
  SaveCfgCommand();
  void on_message() override;
};

class SetRunAppCommand : public Command {
 public:
  SetRunAppCommand(uint8_t mode);
};

class GetHWVCommand : public Command {
 public:
  GetHWVCommand();
  void on_message() override;
};

class GetSWVCommand : public Command {
 public:
  GetSWVCommand();
  void on_message() override;
};

}  // namespace dfrobot_c4001
}  // namespace esphome
