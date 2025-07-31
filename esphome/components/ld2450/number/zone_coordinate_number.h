#pragma once

#include "esphome/components/number/number.h"
#include "../ld2450.h"

namespace esphome {
namespace ld2450 {

class ZoneCoordinateNumber : public number::Number, public Parented<LD2450Component> {
 public:
  ZoneCoordinateNumber(uint8_t zone);

 protected:
  uint8_t zone_;
  void control(float value) override;
};

}  // namespace ld2450
}  // namespace esphome
