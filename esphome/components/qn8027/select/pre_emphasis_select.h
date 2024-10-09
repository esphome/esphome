#pragma once

#include "esphome/components/select/select.h"
#include "../qn8027.h"

namespace esphome {
namespace qn8027 {

class PreEmphasisSelect : public select::Select, public Parented<QN8027Component> {
 public:
  PreEmphasisSelect() = default;

 protected:
  void control(const std::string &value) override;
};

}  // namespace qn8027
}  // namespace esphome
