#pragma once

#include "esphome/components/number/number.h"

namespace esphome {
namespace ld2410 {

class VirtualNumber : public number::Number {
 public:
  VirtualNumber();

 protected:
  void control(float value) override;
};

}  // namespace ld2410
}  // namespace esphome
