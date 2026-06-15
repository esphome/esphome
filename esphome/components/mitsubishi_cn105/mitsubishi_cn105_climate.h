#pragma once

#include "mitsubishi_cn105_component.h"
#include "mitsubishi_cn105.h"

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"

namespace esphome::mitsubishi_cn105 {

class MitsubishiCN105Climate : public climate::Climate, public Component, public Parented<MitsubishiCN105Component> {
 public:
  void setup() override;
  void dump_config() override;

  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  void set_supported_swing_mode(climate::ClimateSwingMode mode);

 protected:
  void apply_values_();

  climate::ClimateSwingModeMask supported_swing_modes_{};
  MitsubishiCN105::VaneMode last_non_swing_vane_mode_{MitsubishiCN105::VaneMode::AUTO};
  MitsubishiCN105::WideVaneMode last_non_swing_wide_vane_mode_{MitsubishiCN105::WideVaneMode::CENTER};
};

}  // namespace esphome::mitsubishi_cn105
