#pragma once

#include "esphome/components/sm10bit_base/sm10bit_base.h"

namespace esphome::sm2335 {

class SM2335 final : public sm10bit_base::Sm10BitBase {
 public:
  using Sm10BitBase::Sm10BitBase;

  void dump_config() override;
};

}  // namespace esphome::sm2335
