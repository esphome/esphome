#pragma once

#include "esphome/components/tmc22xx/tmc22xx.h"

namespace esphome::tmc2209 {

class TMC2209Stepper : public tmc22xx::TMC22XXStepper {
 public:
  void dump_config() override;

 protected:
  uint8_t expected_version_() const override { return 0x21; }
};

}  // namespace esphome::tmc2209
