#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

#include "commands.h"

namespace esphome::dfrobot_sen0395 {

const uint8_t MMWAVE_READ_BUFFER_LENGTH = 255;

// forward declaration due to circular dependency
class DfrobotSen0395Component;

static const uint8_t COMMAND_QUEUE_SIZE = 20;

class CircularCommandQueue {
 public:
  bool enqueue(Command cmd);
  void dequeue();
  bool is_empty() const { return this->front_ == -1; }
  bool is_full() const { return (this->rear_ + 1) % COMMAND_QUEUE_SIZE == this->front_; }
  const Command &front() const { return this->commands_[this->front_]; }

 protected:
  int8_t front_{-1};
  int8_t rear_{-1};
  Command commands_[COMMAND_QUEUE_SIZE];
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

  bool enqueue(CommandType type, uint8_t arg = 0);
  /// Queue a detection range config; a negative value ends the list of segments
  void enqueue_det_range_cfg(float min1, float max1, float min2, float max2, float min3, float max3, float min4,
                             float max4);
  /// Queue an output latency config, both delays in seconds
  void enqueue_set_latency(float delay_after_detection, float delay_after_disappear);
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
  int8_t retries_left_{2};
  CircularCommandQueue cmd_queue_;
  uint32_t ts_last_cmd_sent_{0};
  // Values of the queued range and latency commands, in the sensor's steps of 0.15 m and 0.025 s.
  // A later command replaces them, which leaves the sensor in the same final state.
  int16_t det_range_steps_[8]{};
  uint16_t latency_steps_[2]{};

  uint8_t read_message_();
  uint8_t find_prompt_();
  /// Run the command at the front of the queue; true when it is done
  bool run_command_(const Command &cmd);
  bool run_read_state_();
  /// Handle a reply to a sent command; true when the command is done
  bool handle_reply_(const Command &cmd);
  void format_command_(const Command &cmd, char *buf, size_t size);

  void set_detected_(bool detected);
};

}  // namespace esphome::dfrobot_sen0395
