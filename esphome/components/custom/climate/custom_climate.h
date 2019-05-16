#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"

namespace esphome {
namespace custom {

class CustomClimateConstructor {
 public:
  CustomClimateConstructor(const std::function<std::vector<climate::Climate *>()> &init) { this->climates_ = init(); }

  climate::Climate *get_climate(int i) { return this->climates_[i]; }

 protected:
  std::vector<climate::Climate *> climates_;
};

}  // namespace custom
}  // namespace esphome
