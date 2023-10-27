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

namespace esphome {
namespace dfrobot_sen0395 {

const uint8_t MMWAVE_READ_BUFFER_LENGTH = 255;

// forward declaration due to circular dependency
class DfrobotSen0395Component;

static const uint8_t COMMAND_QUEUE_SIZE = 20;

class CircularCommandQueue {
 public:
  int8_t enqueue(std::unique_ptr<Command> cmd);
  std::unique_ptr<Command> dequeue();
  bool is_empty();
  bool is_full();
  uint8_t process(DfrobotSen0395Component *parent);

 protected:
  int front_{-1};
  int rear_{-1};
  std::unique_ptr<Command> commands_[COMMAND_QUEUE_SIZE];
};

class DfrobotSen0395Component : public uart::UARTDevice, public Component {
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

  int8_t enqueue(std::unique_ptr<Command> cmd);

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
  CircularCommandQueue cmd_queue_;
  uint32_t ts_last_cmd_sent_{0};

  uint8_t read_message_();
  uint8_t find_prompt_();
  uint8_t send_cmd_(const char *cmd, uint32_t duration);

  void set_detected_(bool detected);

  friend class Command;
  friend class ReadStateCommand;
};

}  // namespace dfrobot_sen0395
}  // namespace esphome
