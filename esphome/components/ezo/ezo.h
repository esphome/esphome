#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include <deque>

namespace esphome {
namespace ezo {

class ezo_command {
 public:
  std::string command;
  std::string arguments;
  std::string return_message;
};

/// This class implements support for the EZO circuits in i2c mode
class EZOSensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void loop() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; };

  void set_tempcomp_value(float temp);
  void add_command(std::string command, std::string arguments, std::string return_message) {
    ezo_command *e_command = new ezo_command;
    e_command->command = command;
    e_command->arguments = arguments;
    e_command->return_message = return_message;
    this->commands_.push_back(e_command);
  };

 protected:
  std::deque<ezo_command *> commands_;

  unsigned long start_time_ = 0;
  unsigned long wait_time_ = 0;
  uint16_t state_ = 0;
  float tempcomp_;
};

}  // namespace ezo
}  // namespace esphome
