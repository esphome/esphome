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
  SUB_SENSOR(state_of_health)
  SUB_SENSOR(cycle_count)
  SUB_SENSOR(design_capacity)
  SUB_SENSOR(remaining_capacity)

  void on_line_read(LineContents *line) override;
  void on_soh_read(SohContents *soh) override;

 protected:
  int8_t bat_num_;
};

}  // namespace pylontech
}  // namespace esphome
