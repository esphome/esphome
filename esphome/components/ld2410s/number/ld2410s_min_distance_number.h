#pragma once

#include "../ld2410s.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace ld2410s {

class LD2410SMinDistanceNumber : public number::Number, public Parented<LD2410S> {
 public:
  LD2410SMinDistanceNumber() = default;

 protected:
  void control(float min_distance) override {
#ifdef LD2410S_V2
    this->parent_->set_min_distance(min_distance);
#endif
  }
};

}  // namespace ld2410s
}  // namespace esphome
