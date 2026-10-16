#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

namespace esphome::dfrobot_sen0395 {

static constexpr uint8_t MMWAVE_READ_BUFFER_LENGTH = 255;
static constexpr uint8_t COMMAND_QUEUE_SIZE = 20;

// Keep in sync with the BUSY_ACTIONS table in dfrobot_sen0395.cpp
enum class CommandType : uint8_t {
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

class DfrobotSen0395Component final : public uart::UARTDevice, public Component {
#ifdef USE_SWITCH
  SUB_SWITCH(sensor_active)
  SUB_SWITCH(turn_on_led)
  SUB_SWITCH(presence_via_uart)
  SUB_SWITCH(start_after_boot)
#endif

 public:
  void dump_config() override;
  void loop() override;
  void set_active(bool active) {
    if (active != active_) {
#ifdef USE_SWITCH
      if (this->sensor_active_switch_ != nullptr)
        this->sensor_active_switch_->publish_state(active);
#endif
      active_ = active;
    }
  }
  bool is_active() { return active_; }

  void set_led_active(bool active) {
    if (led_active_ != active) {
#ifdef USE_SWITCH
      if (this->turn_on_led_switch_ != nullptr)
        this->turn_on_led_switch_->publish_state(active);
#endif
      led_active_ = active;
    }
  }
  bool is_led_active() { return led_active_; }

  void set_uart_presence_active(bool active) {
    uart_presence_active_ = active;
#ifdef USE_SWITCH
    if (this->presence_via_uart_switch_ != nullptr)
      this->presence_via_uart_switch_->publish_state(active);
#endif
  }
  bool is_uart_presence_active() { return uart_presence_active_; }

  void set_start_after_boot(bool start) {
    start_after_boot_ = start;
#ifdef USE_SWITCH
    if (this->start_after_boot_switch_ != nullptr)
      this->start_after_boot_switch_->publish_state(start);
#endif
  }
  bool does_start_after_boot() { return start_after_boot_; }

#ifdef USE_BINARY_SENSOR
  void set_detected_binary_sensor(binary_sensor::BinarySensor *detected_binary_sensor) {
    detected_binary_sensor_ = detected_binary_sensor;
  }
#endif

  // Commands run one at a time from a queue, at least 1 s apart, so loop() never blocks
  bool enqueue(CommandType type, uint8_t arg = 0);
  /// Queue a detection range config; segments are used up to the first one with a negative bound
  void enqueue_det_range_cfg(float min1, float max1, float min2, float max2, float min3, float max3, float min4,
                             float max4);
  /// Queue an output latency config, both delays in seconds
  void enqueue_set_latency(float delay_after_detection, float delay_after_disappear);
  /// Queue a setting change, stopping the sensor around it and saving it when the sensor is running
  void enqueue_saved_setting(CommandType type, bool value);
  /// Queue a sensor reset
  void reset_system() { this->enqueue(CommandType::COMMAND_TYPE_RESET_SYSTEM); }

 protected:
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *detected_binary_sensor_{nullptr};
#endif

  bool detected_{false};
  bool active_{false};
  bool led_active_{false};
  bool uart_presence_active_{false};
  bool start_after_boot_{false};
  char read_buffer_[MMWAVE_READ_BUFFER_LENGTH];
  size_t read_pos_{0};
  bool cmd_sent_{false};
  uint8_t retries_left_{2};
  StaticRingBuffer<Command, COMMAND_QUEUE_SIZE> cmd_queue_;
  uint32_t ts_last_cmd_sent_{0};
  // Values of the queued range and latency commands, in the sensor's steps of 0.15 m and 0.025 s.
  // A later command replaces them, which leaves the sensor in the same final state.
  int16_t det_range_steps_[8]{};
  uint16_t latency_steps_[2]{};

  bool read_message_();
  void read_state_();
  /// Run the command at the front of the queue; true when it is done
  bool run_command_(const Command &cmd, uint32_t now);
  /// Handle a reply to a sent command; true when the command is done
  bool handle_reply_(const Command &cmd);
  void format_command_(const Command &cmd, char *buf, size_t size);

  void set_detected_(bool detected);
};

}  // namespace esphome::dfrobot_sen0395
