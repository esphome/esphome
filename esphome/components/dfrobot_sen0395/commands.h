#pragma once

#include <cstdint>

namespace esphome::dfrobot_sen0395 {

// Use command queue and time stamps to avoid blocking.
// When component has run time, check if minimum time (1s) between
// commands has passed. After that run a command from the queue.
enum class CommandType : uint8_t {
  COMMAND_TYPE_READ_STATE,
  COMMAND_TYPE_POWER,
  COMMAND_TYPE_DET_RANGE_CFG,
  COMMAND_TYPE_SET_LATENCY,
  COMMAND_TYPE_SENSOR_CFG_START,
  COMMAND_TYPE_FACTORY_RESET,
  COMMAND_TYPE_RESET_SYSTEM,
  COMMAND_TYPE_SAVE_CFG,
  COMMAND_TYPE_LED_MODE,
  COMMAND_TYPE_UART_OUTPUT,
  COMMAND_TYPE_SENSITIVITY,
};

/// A queued command. arg is the on/off flag, startup mode or sensitivity; the detection range and
/// latency values are held by the component.
struct Command {
  CommandType type;
  uint8_t arg;
};

}  // namespace esphome::dfrobot_sen0395
