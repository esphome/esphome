#pragma once

#include "esphome/components/micronova/micronova.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::micronova {

class MicroNovaSensor : public sensor::Sensor, public MicroNovaListener {
 public:
  MicroNovaSensor(MicroNova *m) : MicroNovaListener(m) {}
  void dump_config() override {
    LOG_SENSOR("", "Micronova sensor", this);
    this->dump_base_config();
  }

  void request_value_from_stove() override {
    this->micronova_->request_address(this->memory_location_, this->memory_address_, this);
  }
  void process_value_from_stove(int value_from_stove) override;

  void set_divisor(uint8_t d) { this->divisor_ = d; }
  void set_fan_speed_offset(uint8_t offset) {
    this->is_fan_speed_ = true;
    this->fan_speed_offset_ = offset;
  }

 protected:
  uint8_t divisor_ = 1;
  uint8_t fan_speed_offset_ = 0;
  bool is_fan_speed_ = false;
};

}  // namespace esphome::micronova
