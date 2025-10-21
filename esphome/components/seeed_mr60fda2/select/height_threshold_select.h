#pragma once

#include "esphome/components/select/select.h"
#include "../seeed_mr60fda2.h"

namespace esphome {
namespace seeed_mr60fda2 {

class HeightThresholdSelect : public select::Select, public Parented<MR60FDA2Component> {
 public:
  HeightThresholdSelect() = default;

 protected:
  void control(const std::string &value) override;
};

}  // namespace seeed_mr60fda2
}  // namespace esphome
