#pragma once

#include "../mitsubishi_cn105_component.h"

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"

namespace esphome::mitsubishi_cn105 {

class MitsubishiCN105VerticalVaneDirectionSelect : public select::Select,
                                                   public Component,
                                                   public Parented<MitsubishiCN105Component> {
 public:
  void setup() override;
  void publish_vane_state(MitsubishiCN105::VaneMode mode);

 protected:
  void control(size_t index) override;
};

}  // namespace esphome::mitsubishi_cn105
