#pragma once

#include "esphome/components/micronova/micronova.h"
#include "esphome/components/number/number.h"

namespace esphome::micronova {

class MicroNovaNumber : public number::Number, public MicroNovaListener {
 public:
  MicroNovaNumber(MicroNova *m) : MicroNovaListener(m) {}
  void dump_config() override;
  void control(float value) override;
  void process_value_from_stove(int value_from_stove) override;

  void set_use_step_scaling(bool v) { this->use_step_scaling_ = v; }

 protected:
  bool use_step_scaling_ = false;
};

}  // namespace esphome::micronova
