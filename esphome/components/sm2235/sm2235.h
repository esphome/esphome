#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sm10bit_base/sm10bit_base.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace sm2235 {

class SM2235 : public sm10bit_base::Sm10BitBase {
 public:
  SM2235() = default;

  void setup() override;
  void dump_config() override;
};

}  // namespace sm2235
}  // namespace esphome
