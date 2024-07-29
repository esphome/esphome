#pragma once

#include "../pylontech.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace pylontech {

class PylontechSensor : public PylontechListener, public Component {
 public:
  PylontechSensor(int8_t bat_num);
  void dump_config() override;

  SUB_SENSOR(voltage)
  SUB_SENSOR(current)
  SUB_SENSOR(temperature)
  SUB_SENSOR(temperature_low)
  SUB_SENSOR(temperature_high)
  SUB_SENSOR(voltage_low)
  SUB_SENSOR(voltage_high)

  SUB_SENSOR(coulomb)
  SUB_SENSOR(mos_temperature)

  void on_line_read(LineContents *line) override;

 protected:
  int8_t bat_num_;
};

}  // namespace pylontech
}  // namespace esphome
