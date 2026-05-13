#pragma once

#include "esphome/components/micronova/micronova.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::micronova {

static const char *const STOVE_STATES[11] = {"Off",
                                             "Start",
                                             "Pellets loading",
                                             "Ignition",
                                             "Working",
                                             "Brazier Cleaning",
                                             "Final Cleaning",
                                             "Standby",
                                             "No pellets alarm",
                                             "No ignition alarm",
                                             "Undefined alarm"};

class MicroNovaTextSensor : public text_sensor::TextSensor, public MicroNovaListener {
 public:
  MicroNovaTextSensor(MicroNova *m) : MicroNovaListener(m) {}
  void dump_config() override;
  void process_value_from_stove(int value_from_stove) override;
};

}  // namespace esphome::micronova
