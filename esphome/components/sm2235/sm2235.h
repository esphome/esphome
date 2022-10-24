#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sm_1024bit_base/sm_1024bit_base.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace sm2235 {

class SM2235 : public sm_1024bit_base::SM_1024BIT_Base {
 public:
  SM2235() = default;

  void setup() override;
  void dump_config() override;
};

}  // namespace sm2235
}  // namespace esphome