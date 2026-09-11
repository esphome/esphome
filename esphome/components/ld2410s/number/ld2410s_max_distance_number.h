#pragma once

#include "../ld2410s.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace ld2410s {

class LD2410SMaxDistanceNumber : public number::Number, public Parented<LD2410S> {
 public:
  LD2410SMaxDistanceNumber() = default;

 protected:
  void control(float max_distance) override {
#ifdef LD2410S_V2
    this->parent_->set_max_distance(max_distance);
#endif
  }
};

}  // namespace ld2410s
}  // namespace esphome
