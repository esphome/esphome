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

class MicroNovaTextSensor : public text_sensor::TextSensor, public MicroNovaSensorListener {
 public:
  MicroNovaTextSensor(MicroNova *m) : MicroNovaSensorListener(m) {}
  void dump_config() override { LOG_TEXT_SENSOR("", "Micronova text sensor", this); }
  void request_value_from_stove() override {
    this->micronova_->request_address(this->memory_location_, this->memory_address_, this);
  }
  void process_value_from_stove(int value_from_stove) override;
};

}  // namespace esphome::micronova
