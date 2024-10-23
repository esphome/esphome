#pragma once

#include "../ld2415h.h"
#include "esphome/components/select/select.h"

namespace esphome {
namespace ld2415h {

class SampleRateSelect : public Component, public select::Select, public Parented<LD2415HComponent> {
 public:
  SampleRateSelect() = default;

 protected:
  void control(const std::string &value) override;
};

}  // namespace ld2415h
}  // namespace esphome
