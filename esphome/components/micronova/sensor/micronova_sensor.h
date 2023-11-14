#pragma once

#include "esphome/components/micronova/micronova.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace micronova {

class MicroNovaSensor : public sensor::Sensor, public MicroNovaSensorListener {
 public:
  MicroNovaSensor(MicroNova *m) : MicroNovaSensorListener(m) {}
  void dump_config() override { LOG_SENSOR("", "Micronova sensor", this); }

  void request_value_from_stove() override {
    this->micronova_->request_address(this->memory_location_, this->memory_address_, this);
  }
  void process_value_from_stove(int value_from_stove) override;

  void set_fan_speed_offset(uint8_t f) { this->fan_speed_offset_ = f; }
  uint8_t get_set_fan_speed_offset() { return this->fan_speed_offset_; }

 protected:
  int fan_speed_offset_ = 0;
};

}  // namespace micronova
}  // namespace esphome
