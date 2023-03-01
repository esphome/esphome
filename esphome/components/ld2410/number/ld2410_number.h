#pragma once

#include "esphome/components/number/number.h"

namespace esphome {
namespace ld2410 {

class LD2410Number : public number::Number {
 public:
  LD2410Number();

 protected:
  void control(float value) override;
};

}  // namespace ld2410
}  // namespace esphome
