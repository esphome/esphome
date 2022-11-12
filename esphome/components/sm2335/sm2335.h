#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sm10bit_base/sm10bit_base.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace sm2335 {

class SM2335 : public sm10bit_base::Sm10BitBase {
 public:
  SM2335() = default;

  void setup() override;
  void dump_config() override;
};

}  // namespace sm2335
}  // namespace esphome
