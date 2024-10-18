#pragma once

#include "esphome/components/select/select.h"
#include "../si4713.h"

namespace esphome {
namespace si4713 {

class AnalogAttenuationSelect : public select::Select, public Parented<Si4713Component> {
 public:
  AnalogAttenuationSelect() = default;

 protected:
  void control(const std::string &value) override;
};

}  // namespace si4713
}  // namespace esphome