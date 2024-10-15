#pragma once

#include "esphome/components/text/text.h"
#include "../si4713.h"

namespace esphome {
namespace si4713 {

class RDSStationText : public text::Text, public Parented<Si4713Component> {
 public:
  RDSStationText() = default;

 protected:
  void control(const std::string &value) override;
};

}  // namespace si4713
}  // namespace esphome
