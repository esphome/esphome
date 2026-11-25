#pragma once

#include "../pylontech.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::pylontech {

class PylontechTextSensor final : public PylontechListener {
 public:
  explicit PylontechTextSensor(uint8_t bat_num) : PylontechListener(bat_num) {}
  void dump_config() override;

  SUB_TEXT_SENSOR(base_state)
  SUB_TEXT_SENSOR(voltage_state)
  SUB_TEXT_SENSOR(current_state)
  SUB_TEXT_SENSOR(temperature_state)

  void on_line_read(LineContents *line) override;
};

}  // namespace esphome::pylontech
