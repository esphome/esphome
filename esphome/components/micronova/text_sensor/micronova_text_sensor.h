#pragma once

#include "esphome/components/micronova/micronova.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace micronova {

class MicroNovaTextSensor : public text_sensor::TextSensor, public MicroNovaSensorListener {
 public:
  MicroNovaTextSensor(MicroNova *m) : MicroNovaSensorListener(m) {}
  void dump_config() override { LOG_TEXT_SENSOR("", "Micronova text sensor", this); }
  void read_value_from_stove() override;
  void set_stove_switch_state(bool v) {}
  bool get_stove_switch_state() { return false; }
};

}  // namespace micronova
}  // namespace esphome
