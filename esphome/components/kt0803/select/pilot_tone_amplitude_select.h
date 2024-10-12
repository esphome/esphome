#pragma once

#include "esphome/components/select/select.h"
#include "../kt0803.h"

namespace esphome {
namespace kt0803 {

class PilotToneAmplitudeSelect : public select::Select, public Parented<KT0803Component> {
 public:
  PilotToneAmplitudeSelect() = default;

 protected:
  void control(const std::string &value) override;
};

}  // namespace kt0803
}  // namespace esphome
