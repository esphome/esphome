#pragma once

#include "esphome/components/micronova/micronova.h"
#include "esphome/components/number/number.h"

namespace esphome::micronova {

class MicroNovaNumber : public number::Number, public MicroNovaListener {
 public:
  MicroNovaNumber() {}
  MicroNovaNumber(MicroNova *m) : MicroNovaListener(m) {}
  void dump_config() override {
    LOG_NUMBER("", "Micronova number", this);
    this->dump_base_config();
  }
  void control(float value) override;
  void request_value_from_stove() override {
    this->micronova_->request_address(this->memory_location_, this->memory_address_, this);
  }
  void process_value_from_stove(int value_from_stove) override;

  void set_use_step_scaling(bool v) { this->use_step_scaling_ = v; }

 protected:
  bool use_step_scaling_ = false;
};

}  // namespace esphome::micronova
