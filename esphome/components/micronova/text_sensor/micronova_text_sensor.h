#pragma once

#include "esphome/components/micronova/micronova.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::micronova {

class MicroNovaTextSensor final : public text_sensor::TextSensor, public MicroNovaListener {
 public:
  MicroNovaTextSensor(MicroNova *m) : MicroNovaListener(m) {}
  void dump_config() override;
  void process_value_from_stove(int value_from_stove) override;
};

}  // namespace esphome::micronova
