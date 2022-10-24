#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sm_1024bit_base/sm_1024bit_base.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace sm2335 {

class SM2335 : public sm_1024bit_base::SM_1024BIT_Base {
 public:
  SM2335() = default;

  void setup() override;
  void dump_config() override;
};

}  // namespace sm2335
}  // namespace esphome