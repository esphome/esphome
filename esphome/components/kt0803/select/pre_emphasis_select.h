#pragma once

#include "esphome/components/select/select.h"
#include "../kt0803.h"

namespace esphome {
namespace kt0803 {

class PreEmphasisSelect : public select::Select, public Parented<KT0803Component> {
 public:
  PreEmphasisSelect() = default;

 protected:
  void control(const std::string &value) override;
};

}  // namespace kt0803
}  // namespace esphome
