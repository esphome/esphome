#pragma once

#include "../pylontech.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace pylontech {

class PylontechSensor : public PylontechListener, public Component {
 public:
  PylontechSensor(int bat_num);
  void dump_config() override;

  PYLONTECH_SENSOR(voltage)
  PYLONTECH_SENSOR(current)
  PYLONTECH_SENSOR(temperature)
  PYLONTECH_SENSOR(temperature_low)
  PYLONTECH_SENSOR(temperature_high)
  PYLONTECH_SENSOR(voltage_low)
  PYLONTECH_SENSOR(voltage_high)

  PYLONTECH_SENSOR(coulomb)
  PYLONTECH_SENSOR(mos_temperature)

  void on_line_read(LineContents *line) override;

 protected:
  int bat_num_;
};

}  // namespace pylontech
}  // namespace esphome
