#pragma once

#include "mitsubishi_cn105_climate.h"

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"

namespace esphome::mitsubishi_cn105 {

class MitsubishiCN105VerticalVaneDirectionSelect : public select::Select, public Parented<MitsubishiCN105Climate> {
 public:
  void control(size_t index) override;
  void publish_vane_state(MitsubishiCN105::VaneMode mode);
};

}  // namespace esphome::mitsubishi_cn105
