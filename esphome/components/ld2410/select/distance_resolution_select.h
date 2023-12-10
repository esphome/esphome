#pragma once

#include "esphome/components/select/select.h"
#include "../ld2410.h"

namespace esphome {
namespace ld2410 {

class DistanceResolutionSelect : public select::Select, public Parented<LD2410Component> {
 public:
  DistanceResolutionSelect() = default;

 protected:
  void control(const std::string &value) override;
};

}  // namespace ld2410
}  // namespace esphome
