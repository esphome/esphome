#pragma once

#include "esphome/components/select/select.h"
#include "mitsubishi_cn105_climate.h"

namespace esphome::mitsubishi_cn105 {

class MitsubishiCN105HorizontalVaneDirectionSelect : public select::Select, public Parented<MitsubishiCN105Climate> {
 public:
  void publish_vane_state(MitsubishiCN105::WideVaneMode mode);

 protected:
  void control(size_t index) override;
};

}  // namespace esphome::mitsubishi_cn105
