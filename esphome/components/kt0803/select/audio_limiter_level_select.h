#pragma once

#include "esphome/components/select/select.h"
#include "../kt0803.h"

namespace esphome {
namespace kt0803 {

class AudioLimiterLevelSelect : public select::Select, public Parented<KT0803Component> {
 public:
  AudioLimiterLevelSelect() = default;

 protected:
  void control(const std::string &value) override;
};

}  // namespace kt0803
}  // namespace esphome
