#pragma once

#include "esphome/core/defines.h"

#ifdef USE_NUMBER

#include "esphome/components/number/number.h"
#include "ld600x.h"

namespace esphome::ld600x {

class LD600XNumber final : public number::Number, public Parented<LD600XComponent> {
 public:
  explicit LD600XNumber(uint8_t kind) : kind_(kind) {}

 protected:
  void control(float value) override;

  uint8_t kind_;
};

}  // namespace esphome::ld600x

#endif  // USE_NUMBER
