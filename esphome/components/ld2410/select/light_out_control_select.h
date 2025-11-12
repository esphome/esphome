#pragma once

#include "esphome/components/select/select.h"
#include "../ld2410.h"

namespace esphome {
namespace ld2410 {

class LightOutControlSelect : public select::Select, public Parented<LD2410Component> {
 public:
  LightOutControlSelect() = default;

 protected:
  void control(size_t index) override;
};

}  // namespace ld2410
}  // namespace esphome
