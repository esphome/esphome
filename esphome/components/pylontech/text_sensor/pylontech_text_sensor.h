#pragma once

#include "../pylontech.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace pylontech {

class PylontechTextSensor : public PylontechListener, public Component {
 public:
  PylontechTextSensor(int bat_num);
  void dump_config() override;

  PYLONTECH_TEXT_SENSOR(base_state)
  PYLONTECH_TEXT_SENSOR(voltage_state)
  PYLONTECH_TEXT_SENSOR(current_state)
  PYLONTECH_TEXT_SENSOR(temperature_state)

  void on_line_read(LineContents *line) override;

 protected:
  int bat_num_;
};

}  // namespace pylontech
}  // namespace esphome
