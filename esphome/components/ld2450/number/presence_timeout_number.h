#pragma once

#include "esphome/components/number/number.h"
#include "../ld2450.h"

namespace esphome {
namespace ld2450 {

class PresenceTimeoutNumber : public number::Number, public Parented<LD2450Component> {
 public:
  PresenceTimeoutNumber() = default;

 protected:
  void control(float value) override;
};

}  // namespace ld2450
}  // namespace esphome
