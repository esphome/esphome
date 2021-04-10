#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include <deque>

namespace esphome {
namespace ezo {

enum EZO_COMMAND_TYPE : uint8_t { EZO_READ = 0, EZO_LED = 1 };
static const char *EzoCommandTypeStrings[] = {"EZO_READ", "EZO_LED"};

class ezo_command {
 public:
  std::string command;
  uint32_t delay_ms = 300;
  bool command_sent = false;
  EZO_COMMAND_TYPE command_type;
};

/// This class implements support for the EZO circuits in i2c mode
class EZOSensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void loop() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; };

  void add_command(std::string command, EZO_COMMAND_TYPE command_type) {
    ezo_command *e_command = new ezo_command;
    e_command->command = command;
    e_command->command_type = command_type;

    this->commands_.push_back(e_command);
  };

  void set_tempcomp_value(float temp);
  void get_state() { this->add_command("R", EZO_COMMAND_TYPE::EZO_READ); }

  // LED
  void set_led_state(bool on);
  void get_led_state() { this->add_command("L,?", EZO_COMMAND_TYPE::EZO_LED); }
  void add_led_state_callback(std::function<void(bool)> &&callback) { this->led_callback_.add(std::move(callback)); }

 protected:
  std::deque<ezo_command *> commands_;
  CallbackManager<void(bool)> led_callback_{};

  unsigned long start_time_ = 0;
};

}  // namespace ezo
}  // namespace esphome
