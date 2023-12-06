#pragma once

#include "esphome/components/select/select.h"
#include "../seeed_mr24hpc1.h"

namespace esphome {
namespace seeed_mr24hpc1 {

class MotionBoundarySelect : public select::Select, public Parented<mr24hpc1Component> {
 public:
  MotionBoundarySelect() = default;

 protected:
  void control(const std::string &value) override;
};

}  // namespace seeed_mr24hpc1
}  // namespace esphome
